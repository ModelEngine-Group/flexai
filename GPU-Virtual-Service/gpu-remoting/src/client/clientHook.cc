#include "../../include/hook/hook.h"


int mainDevIdx; // different ranks(processes) may use different devices
std::vector<ClientEndpoint*> clientEpList;
std::vector<bool> threadValidList;

Configure* config_;
UCPConnection* connectionObj;
SharedMemoryOpt* shmOpt;
GPUidMap* gpuIdMap;
std::once_flag initFlag;
std::once_flag registerFlag;
std::vector<RegisterIOV*> regIOVList;
std::vector<KernelInfo_t*> registeredKernels;
robin_hood::unordered_flat_map<uint64_t, KernelInfo_t*> mapHost2KernelInfo;

bool isReConnected = false;
std::mutex reConnectMutex;
std::shared_mutex threadSharedMutex;
int processID; // the process ID of the current process
int threadNum; // how many sub-threads have been created in this process
int commDevIdx; // the device index for the communicator
thread_local int threadID; // e.g, 1641432
thread_local int ttID; // e.g, 1, 2, 3, ...
thread_local ClientEndpoint* clientEpObj; // the client endpoint object for the current thread
thread_local int myDevIdx;
thread_local int lastReqType = -1;
thread_local bool isTraining = false;
thread_local bool batchCollected = false;
thread_local int curTensorIdx = -1;
thread_local size_t curIter = 0;
thread_local std::vector<TensorInfo_t> tensorByteList;

void Intialize() {
    config_ = new Configure("config.json", true);
    // regIOV = new RegisterIOV();
    regIOVList.reserve(240);
    registeredKernels.reserve(1000);
    clientEpList.reserve(config_->GetReqGPUnum());
    for (int i = 0; i < config_->GetReqGPUnum(); i++) {
        clientEpList.push_back(nullptr);
    }

    processID = getpid();
    std::string shmName = "/flexgv_shm_" + std::to_string(config_->GetClientID()) + "_" + std::to_string(processID) + "_datatype";
    // CurType + dataFeeder IP + dataFeeder Port + CurBatchSize
    shmOpt = new SharedMemoryOpt(shmName, sizeof(BatchInfo_t) + sizeof(int) + IP_STRING_LEN); 
    

    connectionObj = new UCPConnection(true);
#ifdef GV_GPUMAP
#ifndef GV_MSGHANDLER
    gpuIdMap = new GPUidMap(config_->GetReqGPUnum(), config_->GetClientID(), config_->GetProxyIp(), config_->GetProxyPort());
#else
    gpuIdMap = new GPUidMap(config_->GetReqGPUnum(), config_->GetClientID(), config_->GetModel(), config_->GetBatchSize(), config_->GetProxyIp(), config_->GetProxyPort());
#endif
// #elif 
//     mainDevIdx = 0;  // todo: retrieve the available device from shared memory
//     SwitchClientEp(mainDevIdx); // switch to the main device as default
#endif 

    // threadID = static_cast<pid_t>(::syscall(SYS_gettid));
    // threadNum++;
    // ttID = threadNum;
}

void SwitchClientEp(int dev, bool threadInit) { // used for cudaSetDevice or first-time initialization
    ClientEndpoint* localClientEpObj = nullptr;
    {
        std::shared_lock<std::shared_mutex> readLock(threadSharedMutex);
        localClientEpObj = clientEpList[dev];   // read from the list
    }
    if (localClientEpObj == nullptr             // the clientEp for dev is not created yet
        || threadInit) {                        // or the thread is newly created, so need to update the threadID
        std::unique_lock<std::shared_mutex> writeLock(threadSharedMutex);

        // re-check: another thread may have created the object while waiting for the lock
        if (clientEpList[dev] == nullptr) { // not found, create a new one for dev
            uint64_t clientID = config_->GetClientID();
            size_t priority = config_->GetPriority();
            ClientEndpoint* newClientEpObj = new ClientEndpoint(clientID, priority, connectionObj->CreateWorker(true, clientID), dev); 
            clientEpList[dev] = newClientEpObj;
        }
        localClientEpObj = clientEpList[dev];

        if (threadInit) { // used for sub-thread initialization
            threadID = static_cast<pid_t>(::syscall(SYS_gettid));
            threadNum++;
            ttID = threadNum;
            threadValidList.push_back(true);
            tool::Logging(LOG_DEBUG, "SetupClientEpIfNeeded", "new thread#%d(%d) in process(%d) has been created\n", ttID, threadID, processID);
        }
        else {
            threadValidList[ttID - 1] = true;
        }
    }
    clientEpObj = localClientEpObj; // switch to the target object
}

void DestoryResources() {
    if (config_ != nullptr) {
        delete config_;
        config_ = nullptr;
    }
    if (!mapHost2KernelInfo.empty()) {
        for(auto it = mapHost2KernelInfo.begin(); it != mapHost2KernelInfo.end(); it++) {
            free(it->second);
        }
        mapHost2KernelInfo.clear();
    }
    if (!regIOVList.empty()) {
        for(auto it = regIOVList.begin(); it != regIOVList.end(); it++) {
            delete *it;
        }
        regIOVList.clear();
    }
    if (!clientEpList.empty()) {
        for(auto it = clientEpList.begin(); it != clientEpList.end(); it++) {
            delete *it;
        }
        clientEpList.clear();
        delete connectionObj;
        delete shmOpt;
#ifdef GV_GPUMAP
        delete gpuIdMap;
#endif
    }
}

ucs_status_t ServerStatusCallback(void *arg, const void *header, size_t header_length, void *data, size_t length, const ucp_am_recv_param_t *param) {
    const char* myName = "ServerStatusCallback";
    ClientEndpoint* curEp = (ClientEndpoint*)arg;
    printf("%s: header_length: %zu, length: %zu\n", myName, header_length, length);
    RequestIOV reqBuf = RequestIOV(header, header_length, data);
    int serverStatus = reqBuf.Pop<int>();
    if (serverStatus == 0) {
        tool::Logging(LOG_INFO, myName, "sucess: the server has received the request.\n");
    }
    else {
        tool::Logging(LOG_ERROR, myName, "failed: the server's status is %d.\n", serverStatus);
    }
    return UCS_OK;
}

void ClientErrorCallback(void *arg, ucp_ep_h ep, ucs_status_t status) {
    const char* myName = "ClientErrorCallback";
    ConnStatus_t* connStatus = (ConnStatus_t*)arg;
    if (status == UCS_ERR_CONNECTION_RESET) {
        tool::Logging(LOG_INFO, myName, "connection failed: the server has shutdown the connection early.\n");
        connStatus->isClosed = true;
#ifndef GV_BACKUP
        DestoryResources();
        exit(EXIT_FAILURE);
#endif
        // exit(0);
        // ReConnect();
    }
    else if (status == UCS_ERR_ENDPOINT_TIMEOUT) {
        tool::Logging(LOG_ERROR, myName, "connection failed: the connection is timed out.\n");
        DestoryResources();
        exit(EXIT_FAILURE);
    }
    else if (status == UCS_ERR_NOT_CONNECTED) {
        tool::Logging(LOG_ERROR, myName, "connection failed: the server is not connected or the connection is closed.\n");
        DestoryResources();
        exit(EXIT_FAILURE);
    } 
    else {
        tool::Logging(LOG_ERROR, myName, "connection failed: %d(%s)\n",
            status, ucs_status_string(status));
        connStatus->isClosed = true;
    }
}

void CheckTensors(int reqType) {
    if (reqType != CUDA_MEMCPY_ASYNC_H2D && batchCollected == false && isTraining == true) {
        if (lastReqType == CUDA_MEMCPY_ASYNC_H2D && reqType == CUDA_STREAM_SYNCHRONIZE) {
            // maybe next request is cudaMemcpyAsync
            tool::Logging(LOG_DEBUG, HOOK_LOG_TAG, "continue to collect next tensor size\n");
        }
        else if (tensorByteList.size() >= 2) { // at least 2 tensors are collected
            // now cudaMemcpyAsync is no longer called consecutively
            batchCollected = true;
            
            // debug
            for (size_t i = 0; i < tensorByteList.size(); i++) {
                tool::Logging(LOG_INFO, HOOK_LOG_TAG, "batch[%zu]: %zu\n", i, tensorByteList[i].size);
            }
        }
        else {
            tool::Logging(LOG_DEBUG, HOOK_LOG_TAG, "only one tensor is collected, curReqType: %d\n", reqType);
            tensorByteList.clear();
        }
    }
    lastReqType = reqType;
}

bool CheckIteration(void* dst, size_t size) {
    bool isNewIter = false;
    if (isTraining == false) {
    }
    else if (batchCollected == false) {
        tensorByteList.push_back({NULL, size});
    }
    else if (curTensorIdx >= 0 && (lastReqType != CUDA_MEMCPY_ASYNC_H2D && lastReqType != CUDA_STREAM_SYNCHRONIZE)) {
        curTensorIdx = -1;
    }
    else if (tensorByteList[curTensorIdx + 1].size == size) {
        curTensorIdx ++;
        tensorByteList[curTensorIdx].devPtr = dst;
        if (curTensorIdx == tensorByteList.size() - 1) {
            curTensorIdx = -1;
            curIter ++;
            isNewIter = true;
            tool::Logging(HOOK_LOG_TAG, "[pid:%d, tid:%d] curIter: %zu (#%zu in period)\n", processID, threadID, curIter, curIter % BACKUP_PERIOD);
            clientEpObj->SendNewIterRequest(curIter);
        }
    }
    else {
        curTensorIdx = -1;
    }
    return isNewIter;
}
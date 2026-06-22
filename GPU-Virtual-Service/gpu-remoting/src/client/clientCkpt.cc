#include "../../include/clientEndpoint.h"
#include <nccl.h>

void ClientEndpoint::Checkpointing() {
    // std::ofstream outFile("api_records.log", std::ios::out); 
    // for (auto it = reqIOVList.begin(); it != reqIOVList.end(); ++it) {
    //     RequestIOV* tmpReqIOV = &(*it);
    //     outFile << tmpReqIOV->GetRequestType() << std::endl;
    // }
    // outFile.close();

    auto start = std::chrono::high_resolution_clock::now();
    ShrinkReqIOVList();
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;

    tool::Logging(LOG_INFO, myName_, "reqIOVList has %zu requests after shrinking(using %f seconds)\n", reqIOVList.size(), duration.count());
    isReConnected = false; // reset reconnection flag

    // std::ofstream outFile2("api_records_shrink.log", std::ios::out); 
    // for (auto it = reqIOVList.begin(); it != reqIOVList.end(); ++it) {
    //     RequestIOV* tmpReqIOV = &(*it);
    //     outFile2 << tmpReqIOV->GetRequestType();
    //     if (tmpReqIOV->GetRequestType() == CUDA_EVENT_RECORD) {
    //         outFile2 << " " << tmpReqIOV->GetHandleByIndex(0) << ", " << tmpReqIOV->GetHandleByIndex(1);
    //     }
    //     outFile2 << std::endl;
    // }
    // outFile2.close();

    // debug:
    // if (ckptCnt >= 2) {
    //     exit(EXIT_FAILURE);
    // }
    // else {
    //     ckptCnt ++;
    // }

    auto it = reqIOVList.end();
    --it;
    recordedReq = &(*it);

    // ckptIter = 0;
}

void ClientEndpoint::Replay() {
    auto start_replay = std::chrono::high_resolution_clock::now();
    tool::Logging(LOG_INFO, myName_, "start replaying %zu requests from registerIOVList\n", regIOVList.size());
    SendRegisterRequest(this, true);

    // std::ofstream outFile2("api_records_replay.log", std::ios::out);     
    tool::Logging(LOG_INFO, myName_, "start replaying %zu requests from requestIOVList\n", reqIOVList.size());
    for (auto it = reqIOVList.begin(); it != reqIOVList.end(); ++it) {
        RequestIOV* tmpReqIOV = &(*it);
        // outFile2 << tmpReqIOV->GetRequestType() << std::endl;

        if (tmpReqIOV->GetRequestType() == NCCL_GET_UNIQUE_ID) {
            ncclUniqueId uniqueId;
            RequestIOV resBuf = RequestIOV();
            resBuf.Push(uniqueId);
            SendRequestRecvResponse(tmpReqIOV, &resBuf, true, false);
#ifdef GV_GPUMAP
            gpuIdMap->UpdateUniqueID((uint8_t*)&uniqueId, sizeof(ncclUniqueId));
            tool::Logging(LOG_INFO, myName_, "[pid:%d, tid:%d] updated uniqueId\n", _processID, _threadID);
#else
            tool::Logging(LOG_ERROR, myName_, "Replaying for ncclGetUniqueId is not supported in non-GV_GPUMAP mode\n");
            exit(EXIT_FAILURE);
#endif // GV_GPUMAP
            continue;
        } 
        else if (tmpReqIOV->GetRequestType() == NCCL_COMM_INIT_RANK
              || tmpReqIOV->GetRequestType() == NCCL_COMM_INIT_RANK_CONFIG) {
            ncclUniqueId uniqueId;
#ifdef GV_GPUMAP
            gpuIdMap->RequestUniqueID((uint8_t*)&uniqueId, sizeof(ncclUniqueId));
            tmpReqIOV->SetElement(2, &uniqueId, sizeof(ncclUniqueId));
            SendRequest(tmpReqIOV, true, false);
            tool::Logging(LOG_INFO, myName_, "[pid:%d, tid:%d] requested uniqueId\n", _processID, _threadID);
#else
            tool::Logging(LOG_ERROR, myName_, "Replaying for ncclCommInitRank is not supported in non-GV_GPUMAP mode\n");
            exit(EXIT_FAILURE);
#endif // GV_GPUMAP
        }
        else if (tmpReqIOV->GetRequestType() == CUDA_MEMCPY_ASYNC_H2D) {
            SendRequestH2D(tmpReqIOV, (uint8_t*)tmpReqIOV->GetHeaders(), sizeof(cudaMemcpyKind)+sizeof(size_t)+sizeof(uint64_t)+sizeof(uint8_t)+sizeof(uint64_t), false, false);
        }
        else if (tmpReqIOV->GetRequestType() == CUDA_STREAM_SYNCHRONIZE || tmpReqIOV->GetRequestType() == NCCL_COMM_GET_ASYNC_ERROR) {
            int result = 0;
            RequestIOV resBuf = RequestIOV();
            resBuf.Push(&result);
            SendRequestRecvResponse(tmpReqIOV, &resBuf, true, false);
        }
        else {
            SendRequest(tmpReqIOV, true, false);
        }
    }
    // outFile2.close();

    // eval for replay time
    size_t free = 0;
    size_t total = 0;
    int replayFinished = 1;
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_MEM_GET_INFO);
    reqBuf.Push(replayFinished); // dummy 
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(&free);
    resBuf.Push(&total);
    SendRequestRecvResponse(&reqBuf, &resBuf);
    const char* recovery_path = std::getenv("FLEXGV_RECOVERY_CSV_PATH");
    if (recovery_path) {
        auto end_replay = std::chrono::high_resolution_clock::now();
        tool::Logging(LOG_INFO, myName_, "replay finished with %f seconds\n", std::chrono::duration<double>(end_replay - start_replay).count());

        std::ofstream outFile;
        outFile.open(recovery_path, std::ios::app);
        if (outFile.tellp() == 0) {
            outFile << "Time" << std::endl;
        }
        outFile << std::chrono::duration<double>(end_replay - start_replay).count() << std::endl;
        outFile.close();
    }

    tool::Logging(LOG_INFO, myName_, "replay finished\n");
}

void ClientEndpoint::UpdateReqIOVList(RequestIOV* reqBuffer) {
    // if (recordedReq == NULL) {
    //     return; // not started yet
    // }

    int reqType = reqBuffer->GetRequestType();

    if (std::binary_search(std::begin(NotNeedRecordAPIs), std::end(NotNeedRecordAPIs), reqType)) {
        return; // no need to record these apis
    } else {
        RequestIOV* bakReqBuffer = reqBuffer->Clone();
        listLock_.lock();
        reqIOVList.push_back(*bakReqBuffer);
        listLock_.unlock();
    }
}

void ClientEndpoint::UpdateReqIOVList(RequestIOV* reqBuffer, uint8_t* header, size_t headerSize) {
    size_t          memcpyCnt       = *(size_t*)(header + sizeof(cudaMemcpyKind));
    cudaStream_t    memcpyStream    = *(cudaStream_t*)(header + sizeof(cudaMemcpyKind)+sizeof(size_t));
    void*           memcpyDst       = *(void**)(header + sizeof(cudaMemcpyKind)+sizeof(size_t)+sizeof(uint64_t)+sizeof(uint8_t));
    size_t          memsetValue     = 0;
    bool            isTrainTensor   = false;

    for (const auto& tensor : tensorByteList) {
        if (tensor.size == memcpyCnt) {
            return; // means this memcpy request is for train dataloading
        }
    }

    // RequestIOV reqBuf = RequestIOV();
    // reqBuf.PushRequestType(CUDA_MEMSET_ASYNC);
    // reqBuf.Push64BitPointer(memcpyDst);
    // reqBuf.Push(memsetValue);
    // reqBuf.Push(memcpyCnt);
    // reqBuf.Push64BitPointer(memcpyStream);
    // reqBuf.PushThreadID(ttID);
    // RequestIOV* bakReqBuffer = reqBuf.Clone();

    RequestIOV* bakReqBuffer = reqBuffer->Clone(header, headerSize);

    tool::Logging(LOG_DEBUG, myName_, "recorded cudaMemcpyAsync request: dst=%p, count=%zu\n", memcpyDst, memcpyCnt);

    listLock_.lock();
    reqIOVList.push_back(*bakReqBuffer);
    listLock_.unlock();
}

void ClientEndpoint::ShrinkReqIOVList() {
    robin_hood::unordered_flat_set<uint64_t> destroyHandles;
    robin_hood::unordered_flat_set<std::string> setHandles;

    // bool shrinkWhole = reqIOVList.size() > BACKUP_API_MAX_NUM ? true : false;
    bool shrinkWhole = true;
    bool exitFlag = false;
    bool lastNewIter = true;

    int tensorIdx = tensorByteList.size() - 1;
    auto it = reqIOVList.end();
    --it;
    while (!exitFlag) {
        auto eraseIt = it; // avoid invalid iterator
        
        RequestIOV* tmpReqIOV = &(*eraseIt);
        int reqType = tmpReqIOV->GetRequestType();

        bool needErase = false;
        if (tmpReqIOV == recordedReq) {
            needErase = true;
            exitFlag = shrinkWhole == false ? true : false;
        }
        else if (reqType == NEW_ITERATION_REQ) {
            needErase = lastNewIter == false ? true : false;
            lastNewIter = false;
        }
        // else if (tensorIdx >= 0) { 
        //     needErase = false; // reserve the last memcpy request for each tensor
        //     if (reqType == CUDA_MEMCPY_ASYNC_H2D) {
        //         uint8_t* headers = (uint8_t*)tmpReqIOV->GetHeaders();
        //         size_t memcpyCnt = *(size_t*)(headers + sizeof(cudaMemcpyKind));
        //         void* memcpyDst = *(void**)(headers + sizeof(cudaMemcpyKind)+sizeof(size_t)+sizeof(uint64_t)+sizeof(uint8_t));
        //         if (memcpyCnt == tensorByteList[tensorIdx].size && memcpyDst == tensorByteList[tensorIdx].devPtr) {
        //             tool::Logging(LOG_INFO, myName_, "reserved memcpy request for tensor#%d: dst=%p, count=%zu\n", tensorIdx, memcpyDst, memcpyCnt);
        //             tensorIdx--;
        //         }
        //         else {
        //             tool::Logging(LOG_INFO, myName_, "skipped memcpy request for tensor#%d: dst=%p, count=%zu\n", tensorIdx, memcpyDst, memcpyCnt);
        //         }
        //     }
        //     else {
        //         tool::Logging(LOG_INFO, myName_, "skipped request#%d\n", reqType);
        //     }
        // }
        else if (std::binary_search(std::begin(ComputeAPIs), std::end(ComputeAPIs), reqType)
              || reqType == CUDA_MALLOC
              || reqType == CUDA_MEMSET || reqType == CUDA_MEMSET_ASYNC 
              || reqType == CUDA_MEMCPY_H2D || reqType == CUDA_MEMCPY_ASYNC_H2D || reqType == CUDA_MEMCPY_ASYNC_D2D
            ) { //todo: memory-related APIs
            needErase = true;
        }
        else if (std::binary_search(std::begin(DestroyAPIs), std::end(DestroyAPIs), reqType)) {
            if (reqType == NCCL_COMM_DEREGISTER) {
                destroyHandles.insert(tmpReqIOV->GetHandleByIndex(1));
            }
            else {
                destroyHandles.insert(tmpReqIOV->GetHandleByIndex(0));
            }
            needErase = true;
        }
        else if (reqType == CUDNN_SET_STREAM 
              || reqType == CUBLAS_SET_STREAM_V2 || reqType == CUBLAS_SET_MATH_MODE) {
            std::string tmpKey = std::to_string(reqType) + "_" + std::to_string(tmpReqIOV->GetHandleByIndex(0));
            if (setHandles.contains(tmpKey)) {
                needErase = true;
            }
            else {
                setHandles.insert(tmpKey);
            }
        }
        else {
            ucp_dt_iov_t* iovs = tmpReqIOV->GetIOVs();
            size_t iovNum = tmpReqIOV->GetNum();
            for (size_t i = 0; i < iovNum; i++) {
                if (iovs[i].length != sizeof(uint64_t) || *(void**)iovs[i].buffer == NULL) {
                    continue;
                }
                if (destroyHandles.contains(*(uint64_t*)iovs[i].buffer)) {
                    needErase = true;
                    break;
                }
            }
        }

        if (eraseIt != reqIOVList.begin()) {
            it--;
        }
        else {
            exitFlag = true;
        }

        if (needErase) {
            reqIOVList.erase(eraseIt);
            delete tmpReqIOV;
        }
    }

    destroyHandles.clear();
    recordedReq = NULL;
}
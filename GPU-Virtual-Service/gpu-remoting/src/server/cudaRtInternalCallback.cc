#include "../../include/serverEndpoint.h"

#include "../../include/hook/fatBinary.h"

extern "C" {
extern void **__cudaRegisterFatBinary(void *fatCubin);
extern void __cudaRegisterFatBinaryEnd(void **fatCubinHandle);
extern void __cudaUnregisterFatBinary(void **fatCubinHandle);
extern void __cudaRegisterFunction(void **fatCubinHandle, const char *hostFun,
                                   char *deviceFun, const char *deviceName,
                                   int thread_limit, uint3 *tid, uint3 *bid,
                                   dim3 *bDim, dim3 *gDim, int *wSize);
extern void __cudaRegisterVar(void **fatCubinHandle, char *hostVar,
                              char *deviceAddress, const char *deviceName,
                              int ext, size_t size, int constant, int global);
extern void __cudaRegisterSharedVar(void **fatCubinHandle, void **devicePtr,
                                    size_t size, size_t alignment, int storage);
extern void __cudaRegisterShared(void **fatCubinHandle, void **devicePtr);
// extern void __cudaRegisterTexture(void **fatCubinHandle,
//                                   const textureReference *hostVar,
//                                   void **deviceAddress, char *deviceName,
//                                   int dim, int norm, int ext);
// extern void __cudaRegisterSurface(void **fatCubinHandle,
//                                   const surfaceReference *hostVar,
//                                   void **deviceAddress, char *deviceName,
//                                   int dim, int ext);
}

static const char* myName = "CUDARuntimeInternalHandle";

DEFINE_SERVER_AM_CALLBACK(__cudaRegisterHandle) {
    tool::Logging(LOG_REGS, myName, "__CUDA_REGISTER\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RegisterIOV     reqBuf      = RegisterIOV(header, header_length, data);
    size_t          reqNum      = reqBuf.GetRequestNum();
    CUmodule        curModule   = NULL;

    if (!serverEp->initFlag_) {
        cudaSetDevice(serverEp->curDev_);
        cudaFree(0); // explicitly initialize the context for the current device

        int streamPriority = -1 * (int)serverEp->priority_;
        cudaError_t exit_code = cudaStreamCreateWithPriority(&serverEp->defaultStream_, cudaStreamNonBlocking, streamPriority);
        if (exit_code != cudaSuccess) {
            tool::Logging(LOG_ERROR, myName, "cudaStreamCreate failed: %s\n", cudaGetErrorString(exit_code));
            exit(EXIT_FAILURE);
        }
        else {
            tool::Logging(LOG_REGS, myName, "serverEp defaultStream = %p\n", serverEp->defaultStream_);
        }
        // serverEp->defaultStream_ = NULL;

        tool::Logging(myName, "initFlag_ is false\n");
        for (int i = 0; i < 3; i++) {
            serverEp->shmQueSizes_[i] = new SharedMemoryOpt(("/flexgv_shm_"+std::to_string(serverEp->clientID_)+ "_" + std::to_string(serverEp->clientPID_) + "_" + std::to_string(i + 1)).c_str(), sizeof(size_t) * 3, false); // dataFeeder may be the first one to create the shared memory and initialize the space
        }

#ifdef GV_MEMORY_PTX
        serverEp->_cuInfoMap.imageQueue = new moodycamel::BlockingReaderWriterQueue< std::pair<void*, size_t> >(240); 
        serverEp->_cuInfoMap.ptxExtractor = new PTXExtractor(serverEp->_cuInfoMap.imageQueue, serverEp->_cuInfoMap.mapDevName2DevPtr);
        boost::thread_attributes attrs;
        attrs.set_stack_size(THREAD_STACK_SIZE);
        serverEp->_ptxThread = new boost::thread(attrs, boost::bind(&PTXExtractor::Run, serverEp->_cuInfoMap.ptxExtractor));
#endif // GV_MEMORY_PTX

        serverEp->initFlag_ = true;
    }
    else {
        tool::Logging(myName, "initFlag_ is true\n");
    }

    for (size_t i = 0; i < reqNum; i++) {
        int reqType = reqBuf.Pop<int>();
        switch (reqType) {
            case __CUDA_REGISTER_FAT_BINARY: {
                uint64_t    fatBinHandle    = reqBuf.Pop<uint64_t>();
                size_t      fatBinSize      = reqBuf.Pop<size_t>();
                size_t      kernelNum       = reqBuf.Pop<size_t>();
                void*       fatBinText      = reqBuf.AssignAddr<void>();
                void*       image           = fatBinText;
#ifdef GV_MEMORY_PTX
                image = malloc(fatBinSize);
                memcpy(image, fatBinText, fatBinSize);
                serverEp->_fatbinList.push_back(image);

                if (serverEp->_cuInfoMap.imageQueue){ 
                    serverEp->_cuInfoMap.imageQueue->enqueue(std::make_pair(image, fatBinSize));
                }
#endif // GV_MEMORY_PTX

                CUresult    exit_code       = cuModuleLoadData(&curModule, image);
                if (exit_code == CUDA_SUCCESS) {
                    serverEp->_cuInfoMap.mapFatBinHandle2CuModule->insert({fatBinHandle, curModule});
                    tool::Logging(LOG_REGS, myName, "__cudaRegisterFatBinary success: loaded the module(%p) from fatCubin(%p) text\n", curModule, fatBinHandle);
                }
                else {
                    const char* errorStr;
                    cuGetErrorString(exit_code, &errorStr);
                    tool::Logging(LOG_ERROR, myName, "__cudaRegisterFatBinary failed: %s\n", errorStr);
                    return UCS_ERR_IO_ERROR;
                }
                break;
            }
            case __CUDA_REGISTER_FUNCTION: {
                uint64_t    fatBinHandler   = reqBuf.Pop<uint64_t>();
                const char* hostFun         = (const char *)reqBuf.Pop<uint64_t>();
                const char* deviceName      = strdup(reqBuf.AssignCString());
                size_t      paramNum        = reqBuf.Pop<size_t>();
                CUfunction  cuFunc          = NULL;
                CUresult    exit_code       = cuModuleGetFunction(&cuFunc, curModule, deviceName);
                if (exit_code == CUDA_SUCCESS) {
                    serverEp->_cuInfoMap.mapHost2CuFunc->insert({(uint64_t)hostFun, cuFunc});
                    tool::Logging(LOG_REGS, myName, "__cudaRegisterFunction success: registered function(%s) with %zu parameters\n", deviceName, paramNum);
                }
                else {
                    const char* errorStr;
                    cuGetErrorString(exit_code, &errorStr);
                    tool::Logging(LOG_ERROR, myName, "__cudaRegisterFunction failed for function(%s): %s\n", deviceName, errorStr);
                    return UCS_ERR_IO_ERROR;
                }
                break;
            }
            case __CUDA_REGISTER_VAR: {
                uint64_t    fatBinHandler   = reqBuf.Pop<uint64_t>();
                char*       hostVar         = (char*)reqBuf.Pop<uint64_t>();
                const char* deviceName      = strdup(reqBuf.AssignCString());
                CUdeviceptr devPtr          = 0;
                size_t      bytes           = 0;
                CUresult    exit_code   = cuModuleGetGlobal(&devPtr, &bytes, curModule, deviceName);
                if (exit_code == CUDA_SUCCESS) {
                    serverEp->_cuInfoMap.mapHostVar2CuDevPtr->insert({(uint64_t)hostVar, devPtr});
                    tool::Logging(LOG_REGS, myName, "__cudaRegisterVar success: registered variable(hostVar: %p, devName: %s, devPtr: %p)\n", hostVar, deviceName, devPtr);
                }
                else {
                    const char* errorStr;
                    tool::Logging(LOG_ERROR, myName, "__cudaRegisterVar failed for variable(%s): %s\n", deviceName, errorStr);
                    return UCS_ERR_IO_ERROR;
                }
                break;
            }
        }
    }
    tool::Logging(LOG_DEBUG, myName, "__cudaRegister success: registered %zu items, %zu bytes in total\n", reqNum, length);
    return UCS_OK;
}

DEFINE_SERVER_AM_CALLBACK(__cudaRegisterFatBinaryHandle){
    tool::Logging(LOG_REGS, myName, "__CUDA_REGISTER_FAT_BINARY\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        fatBinHandle= reqBuf.Pop<uint64_t>();
    size_t          fatBinSize  = reqBuf.Pop<size_t>();
    size_t          kernelNum   = reqBuf.Pop<size_t>();
    void*           fatBinText  = reqBuf.AssignAddr<void>();
    void*           image       = NULL;
    image = malloc(fatBinSize);
    memcpy(image, fatBinText, fatBinSize);

#ifdef GV_MEMORY_PTX
    if (serverEp->_cuInfoMap.imageQueue){ 
        serverEp->_cuInfoMap.imageQueue->enqueue(std::make_pair(image, fatBinSize));
    }
#endif // GV_MEMORY_PTX

    cudaSetDevice(serverEp->curDev_);


    cudaFree(0); // explicitly initialize the context for device #i

    CUmodule    cuModule    = NULL;
    CUresult    exit_code   = cuModuleLoadData(&cuModule, image);
    if (exit_code == CUDA_SUCCESS) {
        tool::Logging(LOG_REGS, myName, "__cudaRegisterFatBinary success: loaded the module(%p) from fatCubin(%p) text in device #%d\n", cuModule, fatBinHandle, serverEp->curDev_);
        serverEp->_cuInfoMap.mapFatBinHandle2CuModule->insert({fatBinHandle, cuModule});
    }
    else {
        const char* errorStr;
        cuGetErrorString(exit_code, &errorStr);
        tool::Logging(LOG_ERROR, myName, "__cudaRegisterFatBinary failed in device #%d: %s\n", serverEp->curDev_, errorStr);
        return UCS_ERR_IO_ERROR;
    }
    
    // delete[] fatBinText;
    cudaSetDevice(serverEp->curDev_);

    if (param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) {
        free(image);
    }

    return UCS_OK;
}

DEFINE_SERVER_AM_CALLBACK(__cudaRegisterFunctionHandle) {
    tool::Logging(LOG_REGS, myName, "__CUDA_REGISTER_FUNCTION\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        fatBinHandler  = reqBuf.Pop<uint64_t>();
    const char*     hostFun     = (const char *)reqBuf.Pop<uint64_t>();
    const char*     deviceName  = strdup(reqBuf.AssignCString());
    size_t          paramNum    = reqBuf.Pop<size_t>();

        cudaSetDevice(serverEp->curDev_);
        auto        it = serverEp->_cuInfoMap.mapFatBinHandle2CuModule->find(fatBinHandler);
        if (it == serverEp->_cuInfoMap.mapFatBinHandle2CuModule->end()) {
            tool::Logging(LOG_ERROR, myName, "__cudaRegisterFunction failed: unknown handler(%p) in device #%d\n", fatBinHandler, serverEp->curDev_);
            return UCS_ERR_IO_ERROR;
        }
        CUmodule    cuModule    = it->second;
        CUfunction  cuFunc      = NULL;
        CUresult    exit_code   = cuModuleGetFunction(&cuFunc, cuModule, deviceName);
        if (exit_code == CUDA_SUCCESS) {
            serverEp->_cuInfoMap.mapHost2CuFunc->insert({(uint64_t)hostFun, cuFunc});
        }
        else {
            const char* errorStr;
            cuGetErrorString(exit_code, &errorStr);
            tool::Logging(LOG_ERROR, myName, "__cudaRegisterFunction failed for function(%s) in device #%d: %s\n", deviceName, serverEp->curDev_, errorStr);
            return UCS_ERR_IO_ERROR;
        }
    
    tool::Logging(LOG_REGS, myName, "__cudaRegisterFunction success: registered function(%s) with %zu parameters\n", deviceName, paramNum);
    cudaSetDevice(serverEp->curDev_);
    return UCS_OK;
}

void ServerEndpoint::__cudaUnregisterFatBinaryHandle() {
    tool::Logging(myName, "__CUDA_UNREGISTER_FAT_BINARY\n");
 
    for (auto& pair : *_cuInfoMap.mapFatBinHandle2CuModule) {
        CUresult    exit_code   = cuModuleUnload(pair.second);
        if (exit_code != CUDA_SUCCESS) {
            tool::Logging(LOG_ERROR, myName, "__cudaUnregisterFatBinary failed: can not unload the module(%p) from fatCubin(%p) text\n", pair.second, pair.first);
        }
    }
    for (int i = 0; i < _fatbinList.size(); i++) {
        free(_fatbinList[i]);
    }
    for (auto& pair : *_cuInfoMap.mapDevName2DevPtr) {
        auto tmpSet = reinterpret_cast<std::set<std::pair<size_t, size_t>>*>(pair.second);
        delete tmpSet;
    }
    _cuInfoMap.mapFatBinHandle2CuModule->clear();
    _cuInfoMap.mapHost2CuFunc->clear();
    _cuInfoMap.mapHostVar2CuDevPtr->clear();
    
#ifdef GV_MEMORY
    const std::vector<Block_t>& blocks = _cuInfoMap.blockManager->GetBlocks();
    for (int i = 0; i < blocks.size(); i++) {
        if (blocks[i].valid == false) { // maybe have already been freed
            continue;
        }
        cudaError_t exit_code = cudaFree((void*)blocks[i].devPtr); 
        _cuInfoMap.blockManager->ResetBlock(0, i);
    }
    delete _cuInfoMap.blockManager;
#else
    for (size_t blockIdx = 0; blockIdx < _cuInfoMap.blockInfoList.size(); blockIdx++) {
        if (_cuInfoMap.blockInfoList[blockIdx].valid == false) {
            continue;
        }
        cudaError_t exit_code = cudaFree((void*)_cuInfoMap.blockInfoList[blockIdx].devPtr); // maybe have already been freed
        _cuInfoMap.blockInfoList[blockIdx].valid = false;
    }
#endif

    tool::Logging(LOG_DEBUG, myName, "__cudaUnregisterFatBinary success: unloaded all the modules and cleared all the maps of client #%llu\n", clientID_);
    // tool::Logging(LOG_INFO, myName, "blockInfoList size: %zu, handleInfoList size: %zu (valid size: %zu)\n", _cuInfoMap.blockInfoList.size(), _cuInfoMap.handleManager->GetCapacity(), _cuInfoMap.handleManager->GetValidHandleNum());

    cudaStreamDestroy(defaultStream_);

#ifdef GV_BACKUP
    StopBackup();
    StopCommEventMonitor();
#endif // GV_BACKUP

#ifdef GV_HANDLE
    delete _cuInfoMap.handleManager;
#endif // GV_HANDLE

    delete _cuInfoMap.mapFatBinHandle2CuModule;
    delete _cuInfoMap.mapHost2CuFunc;
    delete _cuInfoMap.mapHostVar2CuDevPtr;

    delete _cuInfoMap.ptxExtractor; // ptxExtractor will clean up the imageQueue, mapDevName2DevPtr
    delete _ptxThread;
}

DEFINE_SERVER_AM_CALLBACK(__cudaRegisterVarHandle) {
    tool::Logging(LOG_REGS, myName, "__CUDA_REGISTER_VAR\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        fatBinHandler  = reqBuf.Pop<uint64_t>();

    char*           hostVar     = (char*)reqBuf.Pop<uint64_t>();
    const char*     deviceName  = strdup(reqBuf.AssignCString());
   
    cudaSetDevice(serverEp->curDev_);

    auto        it = serverEp->_cuInfoMap.mapFatBinHandle2CuModule->find(fatBinHandler);
    if (it == serverEp->_cuInfoMap.mapFatBinHandle2CuModule->end()) {
        tool::Logging(LOG_ERROR, myName, "__cudaRegisterVar failed: unknown handler(%p) in device #%d\n", fatBinHandler, serverEp->curDev_);
        return UCS_ERR_IO_ERROR;
    }
    CUmodule    cuModule    = it->second;
    CUdeviceptr devPtr      = 0;
    size_t      bytes       = 0;
    CUresult    exit_code   = cuModuleGetGlobal(&devPtr, &bytes, cuModule, deviceName);
    if (exit_code == CUDA_SUCCESS) {
        tool::Logging(LOG_REGS, myName, "__cudaRegisterVar success: registered variable(hostVar: %p, devName: %s, devPtr: %p in device #%d)\n", hostVar, deviceName, devPtr, serverEp->curDev_);
        serverEp->_cuInfoMap.mapHostVar2CuDevPtr->insert({(uint64_t)hostVar, devPtr});
    }
    else {
        tool::Logging(LOG_ERROR, myName, "__cudaRegisterVar failed: can not load the variable in device #%d\n", serverEp->curDev_);
        return UCS_ERR_IO_ERROR;
    }
    
    cudaSetDevice(serverEp->curDev_);
    return UCS_OK;
}
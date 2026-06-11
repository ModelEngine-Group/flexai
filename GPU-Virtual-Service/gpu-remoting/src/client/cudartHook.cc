#include "../../include/hook/hook.h"
#include "../../include/hook/elfHandle.h"

/* ---- CUDA Runtime Internal API ---- */

extern "C" void** __cudaRegisterFatBinary(void* fatCubin) {
    std::call_once(initFlag, Intialize);
    const char* func_name = "__cudaRegisterFatBinary";
    HookLog(func_name, false, LOG_REGS);
    using func_ptr = void **(*)(void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    FatHeader_t* fatBinHeader = (FatHeader_t*)fatCubin;
    size_t fatBinSize = 0;
    uint8_t* fatBinText = NULL;
    size_t kernelNum = registeredKernels.size();
    if (GetFatbinInfo(fatBinHeader, &registeredKernels, 
                        (uint8_t **)&fatBinText, &fatBinSize) != 0) {
        tool::Logging(LOG_ERROR, func_name, "error getting fatbin info\n");
        return NULL;
    }
    kernelNum = registeredKernels.size() - kernelNum;
    void ** fatBinHandle = (void**)calloc(1, 0x58);
    // uint8_t* fatBinTextCopy = (uint8_t*)malloc(fatBinSize); //todo
    // memcpy(fatBinTextCopy, fatBinText, fatBinSize); // data needs to be in the heap instead of the stack or the data segment for ucx rdma

    regIOVList.push_back(new RegisterIOV());
    regIOVList.back()->PushSubRequestType(__CUDA_REGISTER_FAT_BINARY);
    regIOVList.back()->Push64BitPointer(fatBinHandle);
    regIOVList.back()->Push(fatBinSize);
    regIOVList.back()->Push(kernelNum);
    regIOVList.back()->Push(fatBinText, fatBinSize);
    tool::Logging(LOG_REGS, func_name, "fatBinHandle: %p, fatBinSize: %zu, kernelNum: %zu\n", fatBinHandle, fatBinSize, kernelNum);

    // free(fatBinTextCopy);
    return fatBinHandle;
    //return func_entry(fatCubin);
}

extern "C" void __cudaRegisterVar(void **fatCubinHandle, char *hostVar, char *deviceAddress,
                                                   const char *deviceName, int ext, size_t size, int constant,
                                                   int global) {
    const char* func_name = "__cudaRegisterVar";
    HookLog(func_name, false, LOG_REGS);
    using func_ptr = void (*)(void **, char *, char *, const char *, int, size_t, int, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    tool::Logging(LOG_REGS, func_name, "hostVar: %p , deviceAddr: %p, deviceName: %s , ext: %d , size: %zu , constant: %d , global: %d\n", hostVar, deviceAddress, deviceName, ext, size, constant, global);
    // std::cout << " ,deviceName:" << deviceName << " ,ext:" << ext << " ,size:" << size << " ,constant:" << constant
    //           << " ,global:" << global << std::endl;
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + sizeof(size_t)+strlen(deviceAddress)+1 + sizeof(size_t)+strlen(deviceName)+1 + sizeof(int)*3 + sizeof(size_t));

    regIOVList.back()->PushSubRequestType(__CUDA_REGISTER_VAR);
    regIOVList.back()->Push64BitPointer(fatCubinHandle);
    regIOVList.back()->Push64BitPointer(hostVar);
    regIOVList.back()->PushCString(deviceName);
    tool::Logging(LOG_REGS, func_name, "fatCubinHandle: %p, hostVar: %p", fatCubinHandle, hostVar);
    
    return;
    //return func_entry(fatCubinHandle, hostVar, deviceAddress, deviceName, ext, size, constant, global);
}

extern "C" void __cudaRegisterFunction(void **fatCubinHandle, const char *hostFun, char *deviceFun,
                                                        const char *deviceName, int thread_limit, uint3 *tid,
                                                        uint3 *bid, dim3 *bDim, dim3 *gDim, int *wSize) {
    const char* func_name = "__cudaRegisterFunction";
    HookLog(func_name, false, LOG_REGS);
    using func_ptr =
        void (*)(void **, const char *, char *, const char *, int, uint3 *, uint3 *, dim3 *, dim3 *, int *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // tool::Logging(LOG_REGS, func_name, "fatCubinHandle: %p, hostFun pointer: %p, deviceFun: %s", fatCubinHandle, hostFun, deviceFun);

    KernelInfo_t *info = GetKernelInfoByKernelName(&registeredKernels, (char *)deviceName);
    if (info == NULL) {
        tool::Logging(LOG_ERROR, func_name, "error: kernel info not found\n");
        return;
    }

    regIOVList.back()->PushSubRequestType(__CUDA_REGISTER_FUNCTION);
    regIOVList.back()->Push64BitPointer(fatCubinHandle);
    regIOVList.back()->Push64BitPointer(hostFun);
    regIOVList.back()->PushCString(deviceName);
    regIOVList.back()->Push(info->paramNum);
    tool::Logging(LOG_REGS, func_name, "fatCubinHandle: %p, hostFun: %p, paramNum: %d\n", fatCubinHandle, hostFun, info->paramNum);

    info->host_fun = (void*)hostFun;
    mapHost2KernelInfo.insert({(uint64_t)hostFun, info});

    // // free(newDeviceFun);
    // free(newDeviceName);

    return;
    //return func_entry(fatCubinHandle, hostFun, deviceFun, deviceName, thread_limit, tid, bid, bDim, gDim, wSize);
}

extern "C" void __cudaRegisterFatBinaryEnd(void **fatCubinHandle) {
    const char* func_name = "__cudaRegisterFatBinaryEnd";
    HookLog(func_name, false, LOG_REGS);
    using func_ptr = void (*)(void **);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    regIOVList.back()->PushThreadID(ttID);
    registeredKernels.clear(); // no need to free them since they are stored in the mapHost2KernelInfo

    return func_entry(fatCubinHandle); // it is neccessary to call
}

extern "C" void __cudaUnregisterFatBinary(void **fatCubinHandle) {
    const char* func_name = "__cudaUnregisterFatBinary";
    DestoryResources();
    // HookLog(func_name);
    // using func_ptr = void (*)(void **);
    // auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    // reqBuf.PushRequestType(__CUDA_UNREGISTER_FAT_BINARY);
    // reqBuf.Push64BitPointer(fatCubinHandle);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();

    // std::call_once(destroyFlag, ClientDestory);

    //return func_entry(fatCubinHandle);
}

/* ---- CUDA Runtime Execution API ---- */

cudaError_t cudaLaunchKernel(const void *func, dim3 gridDim, dim3 blockDim, void **args,
                                                         size_t sharedMem, cudaStream_t stream) {
    const char* func_name = "cudaLaunchKernel";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(const void *, dim3, dim3, void **, size_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    auto it = mapHost2KernelInfo.find((uint64_t)func);
    if (it == mapHost2KernelInfo.end()) {
        tool::Logging(LOG_ERROR, func_name, "error: kernel info not found\n");
        return cudaErrorInvalidDeviceFunction;
    }
    KernelInfo_t *info = it->second;
    tool::Logging(LOG_DEBUG, func_name, "kernel info found(name: %s, paramSize: %zd, paramNum: %zd)\n", info->name, info->paramSize, info->paramNum);

    uint8_t* paraValList = (uint8_t*)malloc(info->paramSize);
    for (size_t j = 0; j < info->paramNum; j++) {
        memcpy(paraValList + info->paramOffsets[j], args[j], info->paramSizes[j]);
        // std::cout << "arg[" << j << "]: " << "size=" << info->paramSizes[j] << " " << "offset=" << info->paramOffsets[j] << std::endl;
        // if (info->paramSizes[j] == sizeof(uint64_t)) {
        //     void* actualVal;
        //     memcpy(&actualVal, args[j], info->paramSizes[j]);
        //     printf("arg[%zd]: %p  ", j, actualVal);
        // }
    }
    // printf("\n");

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(dim3)*2 + sizeof(size_t)+info->paramSize + sizeof(size_t) + sizeof(uint64_t) + sizeof(size_t) + 2*sizeof(size_t)+2*sizeof(uint16_t)*info->paramNum);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_LAUNCH_KERNEL);
    reqBuf.Push64BitPointer(func); // func is host function
    reqBuf.PushCString(info->name);
    reqBuf.Push(gridDim);
    reqBuf.Push(blockDim);
    reqBuf.Push(paraValList, info->paramSize); //? <uint8_t>
    reqBuf.Push(sharedMem);
    reqBuf.Push64BitPointer(stream);
    reqBuf.Push(info->paramNum);
    reqBuf.Push(info->paramOffsets, info->paramNum);
    reqBuf.Push(info->paramSizes, info->paramNum);
    
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);

    free(paraValList);

    return cudaSuccess;

    // return func_entry(func, gridDim, blockDim, args, sharedMem, stream);
}

cudaError_t cudaFuncGetAttributes(struct cudaFuncAttributes *attr, const void *func) {
    const char* func_name = "cudaFuncGetAttributes";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(struct cudaFuncAttributes *, const void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_FUNC_GET_ATTRIBUTES);
    reqBuf.Push64BitPointer(func);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(attr);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return cudaSuccess;

    //return func_entry(attr, func);
}

const char *cudaGetErrorName(cudaError_t error) {
    const char* func_name = "cudaGetErrorName";
    HookLog(func_name);
    using func_ptr = const char *(*)(cudaError_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    return func_entry(error);
}

/* ---- CUDA Runtime Memory API ---- */

cudaError_t cudaMalloc(void** devPtr, size_t size) {
    const char* func_name = "cudaMalloc";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void **, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    std::call_once(registerFlag, ClientEndpoint::SendRegisterRequest, clientEpObj, true);  // send the register requests first

    // bool essential = !(tool::CheckPyStackTrace("forward"));
    bool essential = true;
    // std::string filename = "cudaMalloc-trace-" + std::to_string(processID) + ".log";
    if (ttID == 1) {
        if (curIter > 1) { // upon checkpoint, the memory state will be saved
            essential = false;
        } 
        else {
            essential = !(tool::CheckPyStackTrace("forward"));
        }
        // tool::PrintPyStackTrace(filename, true);
    }
    else {
        essential = false;
        // tool::PrintStackTrace(filename, true);
    }

    if (isTraining == false && essential == false) { // todo: maybe ttID > 1
        isTraining = true;
        tool::Logging(LOG_DEBUG, func_name, "start training\n");

        // busy waiting for 10ms, avoid the deadlock
        auto wait_start = std::chrono::high_resolution_clock::now();
        while (true) {
            auto wait_now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(wait_now - wait_start).count();
            if (elapsed >= 1000) {
                break;
            }
        }
    }
    
    RequestIOV reqBuf = RequestIOV();
    // RequestBuffer reqBuf = RequestBuffer(sizeof(size_t));
    reqBuf.PushRequestType(CUDA_MALLOC);
    reqBuf.Push(size);
    reqBuf.Push(essential);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(devPtr); // pass a pointer instead of a value
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    // clientEpObj->ReceiveResponse(sizeof(uint64_t), devPtr); // pass a pointer that refers to a unit storing the CUDA memory address
    tool::Logging(LOG_DEBUG, func_name, "[pid:%d, tid:%d, ttid:%d] allocated devPtr = %p, size = %zu, essential = %d\n\n", processID, threadID, ttID, *devPtr, size, essential);
    return cudaSuccess;

    //return func_entry(devPtr, size);
}

cudaError_t cudaMallocHost(void **ptr, size_t size) {
    const char* func_name = "cudaMallocHost";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void **, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    *ptr = malloc(size);
    return cudaSuccess;

    //return func_entry(ptr, size);
}

cudaError_t cudaHostAlloc(void **pHost, size_t size, unsigned int flags) {
    const char* func_name = "cudaHostAlloc";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void **, size_t, unsigned int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    if (flags != 0x00) { // only support cudaHostAllocDefault
        return cudaErrorInvalidValue;
    }

    *pHost = malloc(size);
    return cudaSuccess;

    //return func_entry(pHost, size, flags);
}

cudaError_t cudaMemset(void *devPtr, int value, size_t count) {
    const char* func_name = "cudaMemset";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void *, int, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(int) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_MEMSET);
    reqBuf.Push64BitPointer(devPtr);
    reqBuf.Push(value);
    reqBuf.Push(count);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess; 

    //return func_entry(devPtr, value, count);
}

cudaError_t cudaMemsetAsync(void *devPtr, int value, size_t count, cudaStream_t stream) {
    const char* func_name = "cudaMemsetAsync";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void *, int, size_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(int) + sizeof(size_t) + sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_MEMSET_ASYNC);
    reqBuf.Push64BitPointer(devPtr);
    reqBuf.Push(value);
    reqBuf.Push(count);
    reqBuf.Push64BitPointer(stream);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess; 

    //return func_entry(devPtr, value, count);
}

cudaError_t cudaMemcpy(void *dst, const void *src, size_t count, enum cudaMemcpyKind kind) {
    const char* func_name = "cudaMemcpy";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void *, const void *, size_t, enum cudaMemcpyKind);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaMemcpyKind) + sizeof(size_t) + sizeof(uint64_t) + sizeof(uint64_t));
    
    RequestIOV reqBuf = RequestIOV();
    if (kind == cudaMemcpyHostToDevice) {
        // uint8_t headers[sizeof(cudaMemcpyKind) + sizeof(size_t) + sizeof(uint64_t) + sizeof(int)];
        uint8_t headers[sizeof(cudaMemcpyKind) + sizeof(size_t) + sizeof(uint64_t)];
        size_t headerSize = 0;
        memcpy(headers + headerSize, &kind, sizeof(cudaMemcpyKind));
        headerSize += sizeof(cudaMemcpyKind);
        memcpy(headers + headerSize, &count, sizeof(size_t));
        headerSize += sizeof(size_t);
        memcpy(headers + headerSize, &dst, sizeof(uint64_t));
        headerSize += sizeof(uint64_t);

        reqBuf.PushRequestType(CUDA_MEMCPY_H2D);
        reqBuf.Push((uint8_t*)src, count);

        clientEpObj->SendRequestH2D(&reqBuf, headers, headerSize);
        tool::Logging(LOG_DEBUG, func_name, "H2D: Host -> Device(%p) with %zu B\n", dst, count);
    }
    else if (kind == cudaMemcpyDeviceToHost) {
        reqBuf.PushRequestType(CUDA_MEMCPY_D2H);
        reqBuf.Push(kind);
        reqBuf.Push(count);
        reqBuf.Push64BitPointer(src);
        
        tool::Logging(LOG_DEBUG, func_name, "D2H: Device(%p) -> Host with %zu B\n", src, count);

        RequestIOV resBuf = RequestIOV();
        resBuf.Push((uint8_t*)dst, count);
        clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    }
    else if (kind == cudaMemcpyDeviceToDevice) {
        reqBuf.PushRequestType(CUDA_MEMCPY_D2D);
        reqBuf.Push(kind);
        reqBuf.Push(count);
        reqBuf.Push64BitPointer(src);
        reqBuf.Push64BitPointer(dst);
        clientEpObj->SendRequest(&reqBuf);
        tool::Logging(LOG_DEBUG, func_name, "D2D: Device(%p) -> Device(%p) with %zu B\n", src, dst, count);
    }
    else {
        tool::Logging(LOG_ERROR, func_name, "failed: invalid direction: %d\n", kind);
        return cudaErrorInvalidMemcpyDirection;
        //todo;
    }
    
    return cudaSuccess;
    //return func_entry(dst, src, count, kind);
}

cudaError_t cudaMemcpyAsync(void *dst, const void *src, size_t count,
                                                        enum cudaMemcpyKind kind, cudaStream_t stream) {
    const char* func_name = "cudaMemcpyAsync";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void *, const void *, size_t, enum cudaMemcpyKind, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    RequestIOV reqBuf = RequestIOV();
    if (kind == cudaMemcpyHostToDevice) {
        BatchInfo_t batchInfo;
        if (clientEpObj->_shmOpt->ReadCurBatchInfo(&batchInfo) == false) {
            tool::Logging(LOG_ERROR, func_name, "failed: cannot read CurType from shared memory\n");
            return cudaErrorUnknown;
        }
        else {
            tool::Logging(LOG_DEBUG, func_name, "batchInfo curType: %d, curSize: %zu\n", batchInfo.curType, batchInfo.curBatchSize);
        }

        // tool::PrintPyStackTrace("cudaMemcpy-trace.log", true);

        uint8_t headers[sizeof(cudaMemcpyKind)+sizeof(size_t)+sizeof(uint64_t)+sizeof(uint8_t)+sizeof(uint64_t)];
        size_t headerSize = 0;
        memcpy(headers + headerSize, &kind, sizeof(cudaMemcpyKind));
        headerSize += sizeof(cudaMemcpyKind);
        memcpy(headers + headerSize, &count, sizeof(size_t));
        headerSize += sizeof(size_t);
        memcpy(headers + headerSize, &stream, sizeof(uint64_t));
        headerSize += sizeof(uint64_t);
        memcpy(headers + headerSize, &batchInfo.curType, sizeof(uint8_t));
        headerSize += sizeof(uint8_t);
        memcpy(headers + headerSize, &dst, sizeof(uint64_t));
        headerSize += sizeof(uint64_t);
        
        reqBuf.PushRequestType(CUDA_MEMCPY_ASYNC_H2D);
        if (batchInfo.curType != MEMCPY_OTHER) { // with data preloading optimization, data is already in remote node
            clientEpObj->_copySize += count;
            if (clientEpObj->_copySize >= batchInfo.curBatchSize) {
                clientEpObj->_shmOpt->ResetBatchInfo(MEMCPY_OTHER); // avoid conflicts with copy model parameters in DDP
                clientEpObj->_copySize = 0;
            }
        }
        else { // without data preloading, data will be sent to remote node
            reqBuf.Push((uint8_t*)src, count);
        }
        clientEpObj->SendRequestH2D(&reqBuf, headers, headerSize);
        tool::Logging(LOG_DEBUG, func_name, "H2D: Host -> Device(%p) with %zu B\n", dst, count);

        CheckIteration(dst, count);
    }
    else if (kind == cudaMemcpyDeviceToHost) {
        cudaStreamSynchronize(stream); // for eager/rndv synchronization between host and device
        reqBuf.PushRequestType(CUDA_MEMCPY_ASYNC_D2H);
        reqBuf.Push(kind);
        reqBuf.Push(count);
        reqBuf.Push64BitPointer(stream);
        reqBuf.Push64BitPointer(src);

        tool::Logging(LOG_DEBUG, func_name, "D2H: Device(%p) -> Host with %zu B\n", src, count);

        RequestIOV resBuf = RequestIOV();
        resBuf.Push((uint8_t*)dst, count);
        clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    }
    else if (kind == cudaMemcpyDeviceToDevice) {
        reqBuf.PushRequestType(CUDA_MEMCPY_ASYNC_D2D);
        reqBuf.Push(kind);
        reqBuf.Push(count);
        reqBuf.Push64BitPointer(stream);
        reqBuf.Push64BitPointer(src);
        reqBuf.Push64BitPointer(dst);
        clientEpObj->SendRequest(&reqBuf);
        tool::Logging(LOG_DEBUG, func_name, "D2D: Device(%p) -> Device(%p) with %zu B\n", src, dst, count);
    }
    else {
        tool::Logging(LOG_ERROR, func_name, "failed: invalid direction(%d)\n", kind);
        return cudaErrorInvalidMemcpyDirection;
    }

    return cudaSuccess;
    //return func_entry(dst, src, count, kind);
}

cudaError_t cudaMemcpyToSymbol(const void *symbol, const void *src, size_t count,
                                                           size_t offset, enum cudaMemcpyKind kind) {
    const char* func_name = "cudaMemcpyToSymbol";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(const void *, const void *, size_t, size_t, enum cudaMemcpyKind);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaMemcpyKind) + sizeof(size_t)*2 + sizeof(uint64_t) + sizeof(size_t)+count);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_MEMCPY_TO_SYMBOL);
    reqBuf.Push(kind);
    reqBuf.Push(count);
    reqBuf.Push(offset);

    if (kind == cudaMemcpyHostToDevice || kind == cudaMemcpyDefault) {
        reqBuf.Push64BitPointer(symbol);
        reqBuf.Push((uint8_t*)src, count);
        // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
        // clientEpObj->SendRequest();
        clientEpObj->SendRequest(&reqBuf);

        // clientEpObj->AddIOV(count, src);
        // clientEpObj->SendRequest(false);
        tool::Logging(LOG_DEBUG, func_name, "send host data to remote device symbol\n");
    }
    else {
        return cudaErrorInvalidMemcpyDirection;
    }

    return cudaSuccess;
    //return func_entry(symbol, src, count, offset, kind);
}

cudaError_t cudaMemGetInfo(size_t *free, size_t *total) {
    const char* func_name = "cudaMemGetInfo";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(size_t *, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(size_t) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_MEM_GET_INFO);
    int tmpDev = 0;
    reqBuf.Push(tmpDev); // dummy 

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(free);
    resBuf.Push(total);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return cudaSuccess;

    //return func_entry(free, total);
}

cudaError_t cudaFree(void *devPtr) {
    const char* func_name = "cudaFree";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_FREE);
    reqBuf.Push64BitPointer(devPtr);
    
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess;
    //return func_entry(devPtr);
}

cudaError_t cudaFreeHost(void *ptr) {
    const char* func_name = "cudaFreeHost";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    free(ptr);
    return cudaSuccess;
    //return func_entry(ptr);
}

/* ---- CUDA Runtime Device API ---- */

cudaError_t cudaGetDeviceProperties(struct cudaDeviceProp *prop, int device) {
    const char* func_name = "cudaGetDeviceProperties";
    HookLog(func_name, false);
    using func_ptr = cudaError_t (*)(struct cudaDeviceProp *, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
#ifdef GV_GPUMAP
    return (gpuIdMap->GetGPUprop(device, prop)) ? cudaSuccess : cudaErrorInvalidDevice;
#endif 
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_GET_DEVICE_PROPERTIES);
    int gpuIdInNode = device;
    reqBuf.Push(gpuIdInNode);

    // tool::Logging(LOG_INFO, func_name, "device = %d, gpuIdInNode = %d\n", device, gpuIdInNode);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(prop);

    SwitchClientEp(device);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    SwitchClientEp(myDevIdx);

    return cudaSuccess;
    
    //return func_entry(prop, device);
}

cudaError_t cudaDeviceGetAttribute(int *value, enum cudaDeviceAttr attr, int device) {
    const char* func_name = "cudaDeviceGetAttribute";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(int *, enum cudaDeviceAttr, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

/* local optimization
    RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    reqBuf.PushRequestType(CUDA_GET_DEVICE_PROPERTIES);
    reqBuf.Push(device);
    clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    clientEpObj->SendRequest();

    struct cudaDeviceProp deviceProp;
    clientEpObj->ReceiveResponse(sizeof(struct cudaDeviceProp), &deviceProp);

    switch (attr) {
        case cudaDevAttrMaxThreadsPerBlock:
            *value = deviceProp.maxThreadsPerBlock;
            break;
        case cudaDevAttrMaxBlockDimX:
            *value = deviceProp.maxThreadsDim[0];
            break;
        case cudaDevAttrMaxBlockDimY:
            *value = deviceProp.maxThreadsDim[1];
            break;
        case cudaDevAttrMaxBlockDimZ:
            *value = deviceProp.maxThreadsDim[2];
            break;
        case cudaDevAttrMaxGridDimX:
            *value = deviceProp.maxGridSize[0];
            break;
        case cudaDevAttrMaxGridDimY:
            *value = deviceProp.maxGridSize[1];
            break;
        case cudaDevAttrMaxGridDimZ:
            *value = deviceProp.maxGridSize[2];
            break;
        case cudaDevAttrMaxSharedMemoryPerBlock:
            *value = static_cast<int>(deviceProp.sharedMemPerBlock);
            break;
        case cudaDevAttrTotalConstantMemory:
            *value = static_cast<int>(deviceProp.totalConstMem);
            break;
        case cudaDevAttrWarpSize:
            *value = deviceProp.warpSize;
            break;
        case cudaDevAttrComputeMode:
            *value = deviceProp.computeMode;
            break;
        case cudaDevAttrComputeCapabilityMajor:
            *value = deviceProp.major;
            break;
        case cudaDevAttrComputeCapabilityMinor:
            *value = deviceProp.minor;
            break;
        case cudaDevAttrMultiProcessorCount:
            *value = deviceProp.multiProcessorCount;
            break;
        case cudaDevAttrClockRate:
            *value = deviceProp.clockRate;
            break;
        case cudaDevAttrIntegrated:
            *value = deviceProp.integrated;
            break;
        // ... Handling of other attributes ...
        default:
            // Handles unknown or unsupported attributes
            *value = -1;
            break;
    }

    if (*value == -1) {
        tool::Logging(LOG_ERROR, func_name, "unsupported attribute\n");
        return cudaErrorInvalidValue;
    }
    else {
        return cudaSuccess;
    }
*/
    // RequestBuffer reqBuf = RequestBuffer(sizeof(enum cudaDeviceAttr) + sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_DEVICE_GET_ATTRIBUTE);
    reqBuf.Push(attr);
    int gpuIdInNode = device;
#ifdef GV_GPUMAP
    gpuIdMap->GetGPUId(device, &gpuIdInNode);
#endif
    reqBuf.Push(gpuIdInNode);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(value);
    int oldDevice = clientEpObj->_myDevIdx;
    SwitchClientEp(device);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    SwitchClientEp(oldDevice);

    return cudaSuccess;
    
    //return func_entry(value, attr, device);
}

cudaError_t cudaGetDeviceCount(int* count) {
    const char* func_name = "cudaGetDeviceCount";

    using func_ptr = cudaError_t (*)(int *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    if (connectionObj == nullptr) {
        // when creating the ucp connection, cudaGetDeviceCount() will be called
        *count = 0;
    }
    else {
        HookLog(func_name, false);
        // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
        // RequestIOV reqBuf = RequestIOV();
        // reqBuf.PushRequestType(CUDA_GET_DEVICE_COUNT);
        // int tmpDev = 0;
        // reqBuf.Push(tmpDev); // dummy

        // RequestIOV resBuf = RequestIOV();
        // resBuf.Push(count);
        // clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
        // Configure* config = new Configure("config.json", true);
        *count = config_->GetReqGPUnum();

        // *count = 1;
    }
    return cudaSuccess;

    //return func_entry(count);
}

cudaError_t cudaGetDevice(int *device) {
    const char* func_name = "cudaGetDevice";
    HookLog(func_name, false);
    using func_ptr = cudaError_t (*)(int *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    *device = myDevIdx; // return the mapped device id

    tool::Logging(LOG_DEBUG, func_name, "[pid:%d, tid:%d] device = %d\n", processID, threadID, *device);

    // *device = 0;
    // todo: recv device id from remote gpu node
    return cudaSuccess;

    //return func_entry(device);
}

cudaError_t cudaSetDevice(int device) {
    const char* func_name = "cudaSetDevice";
    HookLog(func_name, false);
    using func_ptr = cudaError_t (*)(int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // void* callstack[128];
    // int frames = backtrace(callstack, 128);
    // char** strs = backtrace_symbols(callstack, frames);
    // for (int i = 0; i < frames; ++i) {
    //     std::cout << strs[i] << std::endl;
    // }
    // free(strs);

    // exit(EXIT_FAILURE);

    tool::Logging(LOG_DEBUG, func_name, "[pid:%d, tid:%d, ttid:%d] device = %d\n", processID, threadID, ttID, device);

    myDevIdx = device;
    // if (clientEpObj) { // if not null, means that the current clientEp has been initialized and ready to participate in the training
    //     SwitchClientEp(device);
    // }

    // RequestIOV reqBuf = RequestIOV();
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    // reqBuf.PushRequestType(CUDA_SET_DEVICE);
    // reqBuf.Push(device);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess;

    //return func_entry(device);    
}

cudaError_t cudaDeviceSynchronize() {
    const char* func_name = "cudaDeviceSynchronize";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_DEVICE_SYNCHRONIZE);
    int tmpDev = 0;
    reqBuf.Push(tmpDev); // dummy
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    // func_entry();
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess;
    //return func_entry();
}

/* ---- CUDA Runtime Stream API ---- */

cudaError_t cudaStreamIsCapturing(cudaStream_t stream, enum cudaStreamCaptureStatus *pCaptureStatus) {
    const char* func_name = "cudaStreamIsCapturing";
    HookLog(func_name, false);
    using func_ptr = cudaError_t (*)(cudaStream_t, enum cudaStreamCaptureStatus *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaStream_t) + sizeof(enum cudaStreamCaptureStatus *));

    if (clientEpObj == nullptr) {
        *pCaptureStatus = cudaStreamCaptureStatusNone;
    }
    else {
        RequestIOV reqBuf = RequestIOV();
        reqBuf.PushRequestType(CUDA_STREAM_IS_CAPTURING);
        reqBuf.Push64BitPointer(stream);

        RequestIOV resBuf = RequestIOV();
        resBuf.Push(pCaptureStatus);
        clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    }

    tool::Logging(LOG_DEBUG, func_name, "success, stream = %p, captureStatus = %d\n", stream, *pCaptureStatus);
    return cudaSuccess;

    //return func_entry(stream, pCaptureStatus);
}

cudaError_t cudaStreamGetCaptureInfo(cudaStream_t stream, enum cudaStreamCaptureStatus *pCaptureStatus,
                                                          unsigned long long *pId) {
    const char* func_name = "cudaStreamGetCaptureInfo";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t, enum cudaStreamCaptureStatus *, unsigned long long *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaStream_t) + sizeof(enum cudaStreamCaptureStatus *) + sizeof(unsigned long long *));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_GET_CAPTURE_INFO);
    reqBuf.Push64BitPointer(stream);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(pCaptureStatus);
    resBuf.Push(pId);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "success, status = %d, pId = %llu\n", *pCaptureStatus, *pId);

    return cudaSuccess;
    
    //return func_entry(stream, pCaptureStatus, pId);
}

cudaError_t cudaStreamWaitEvent(cudaStream_t stream, cudaEvent_t event,
                                                            unsigned int flags) {
    const char* func_name = "cudaStreamWaitEvent";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t, cudaEvent_t, unsigned int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaStream_t) + sizeof(cudaEvent_t) + sizeof(uint));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_WAIT_EVENT);
    reqBuf.Push64BitPointer(stream);
    reqBuf.Push64BitPointer(event);
    reqBuf.Push(flags);

    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();    
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess;
    //end
    //return func_entry(stream, event, flags);
}

cudaError_t cudaStreamSynchronize(cudaStream_t stream) {
    const char* func_name = "cudaStreamSynchronize";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaStream_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_SYNCHRONIZE);
    reqBuf.Push64BitPointer(stream);  

    cudaError_t result = cudaSuccess;
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(&result);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    // clientEpObj->SendRequest(&reqBuf);
    // tool::Logging(LOG_DEBUG, func_name, "send cudaStreamSynchronize request, stream = %p\n", stream);

    return result;
    
    //return func_entry(stream);
}

cudaError_t cudaStreamCreate(cudaStream_t *pStream) {
    const char* func_name = "cudaStreamCreate";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer();
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_CREATE);
    *pStream = NULL;
    reqBuf.Push(pStream);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(pStream);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    tool::Logging(LOG_DEBUG, func_name, "create stream success, pStream =  %p\n", *pStream);

    return cudaSuccess;
}

cudaError_t cudaStreamCreateWithFlags(cudaStream_t *pStream, unsigned int flags) {
    const char* func_name = "cudaStreamCreateWithFlags";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t *, unsigned int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_CREATE_WITH_FLAGS);
    *pStream = NULL;
    reqBuf.Push(pStream);
    reqBuf.Push(flags);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(pStream);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "create stream with flags success, pStream =  %p\n", *pStream);

    return cudaSuccess;

    //return func_entry(pStream, flags);
}

cudaError_t cudaStreamCreateWithPriority(cudaStream_t *pStream, unsigned int flags,
                                                                     int priority) {
    const char* func_name = "cudaStreamCreateWithPriority";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t *, unsigned int, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint) + sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_CREATE_WITH_PRIORITY);
    *pStream = NULL;
    reqBuf.Push(pStream);
    reqBuf.Push(flags);
    reqBuf.Push(priority);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(pStream);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "create stream with priority success, pStream =  %p\n", *pStream);

    return cudaSuccess;
    //return func_entry(pStream, flags, priority);
}

cudaError_t cudaStreamDestroy(cudaStream_t stream) {
    const char* func_name = "cudaStreamDestroy";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaStream_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_STREAM_DESTROY);
    reqBuf.Push64BitPointer(stream);

    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess;
    //return func_entry(stream);
}

/* ---- CUDA Runtime Event API ---- */

cudaError_t cudaEventRecord(cudaEvent_t event, cudaStream_t stream) {
    const char* func_name = "cudaEventRecord";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaEvent_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // uint32_t size = sizeof(uint64_t) * 2;
    // RequestBuffer reqBuf = RequestBuffer(size);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_EVENT_RECORD);
    reqBuf.Push64BitPointer(event);
    reqBuf.Push64BitPointer(stream);

    // clientEpObj->AddIOV(reqBuf.GetSize(),reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return cudaSuccess;
    // return func_entry(event, stream);
}

cudaError_t cudaEventCreate(cudaEvent_t *event) {
    const char* func_name = "cudaEventCreate";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaEvent_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // uint32_t size = 0;
    // RequestBuffer reqBuf = RequestBuffer(size);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_EVENT_CREATE);
    *event = NULL;
    reqBuf.Push(event);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(event);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "allocated eventPtr = %p\n", *event);
    return cudaSuccess;
    // return func_entry(event);
}

cudaError_t cudaEventCreateWithFlags(cudaEvent_t *event, unsigned int flags) {
    const char* func_name = "cudaEventCreateWithFlags";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaEvent_t *, unsigned int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // uint32_t size = sizeof(unsigned int);
    // RequestBuffer reqBuf = RequestBuffer(size);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_EVENT_CREATE_WITH_FLAGS);
    *event = NULL;
    reqBuf.Push(event);
    reqBuf.Push(flags);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(event);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    tool::Logging(LOG_DEBUG, func_name, "allocated eventPtr = %p\n", *event);
    return cudaSuccess;
    // return func_entry(event, flags);
}

cudaError_t cudaEventQuery(cudaEvent_t event) { //todo: DDP
    const char* func_name = "cudaEventQuery";
    // tool::Logging(LOG_DEBUG, func_name, "[pid:%d, tid:%d] ready to query event, eventPtr = %p\n", processID, threadID, event);
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaEvent_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_EVENT_QUERY);
    reqBuf.Push64BitPointer(event);

    cudaError_t result = cudaSuccess;

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(&result);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    if(result == cudaSuccess)
        tool::Logging(LOG_DEBUG, func_name, "success\n");
    else    
        tool::Logging(LOG_DEBUG, func_name, "error: %s\n",cudaGetErrorName(result));
    return result; // return the event query result

    // //return func_entry(event);
}

cudaError_t cudaEventDestroy(cudaEvent_t event) {
    const char* func_name = "cudaEventDestroy";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(cudaEvent_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    
    // uint32_t size = sizeof(uint64_t);
    // RequestBuffer reqBuf = RequestBuffer(size);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_EVENT_DESTROY);
    reqBuf.Push64BitPointer(event);
    // clientEpObj->AddIOV(reqBuf.GetSize(),reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);

    tool::Logging(LOG_DEBUG, func_name, "destroyed eventPtr = %p\n", event);

    return cudaSuccess;
    // return func_entry(event);
}

cudaError_t cudaEventElapsedTime(float *ms, cudaEvent_t start, cudaEvent_t end) {
    const char* func_name = "cudaEventElapsedTime";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(float *, cudaEvent_t, cudaEvent_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) * 2);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_EVENT_ELAPSED_TIME);
    reqBuf.Push64BitPointer(start);
    reqBuf.Push64BitPointer(end);

    cudaError_t result = cudaSuccess;
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(result);
    resBuf.Push(ms);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return result;
    //return func_entry(ms, start, end);
}

/* ---- CUDA Runtime Other API ---- */

cudaError_t cudaGetLastError() {
    const char* func_name = "cudaGetLastError";
    HookLog(func_name, false);
    using func_ptr = cudaError_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    return cudaSuccess; //todo: return the remote device error
}

cudaError_t cudaPeekAtLastError() {
    const char* func_name = "cudaPeekAtLastError";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));
    return cudaSuccess; //todo: return the remote device error
}

cudaError_t cudaOccupancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, const void *func,
                                                          int blockSize, size_t dynamicSMemSize) {
    const char* func_name = "cudaOccupancyMaxActiveBlocksPerMultiprocessor";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(int *, const void *, int, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    std::call_once(registerFlag, ClientEndpoint::SendRegisterRequest, clientEpObj, true);  // send the register requests first

    // RequestBuffer reqBuf = RequestBuffer(sizeof(int)*2 + sizeof(uint64_t) + sizeof(int) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR);
    reqBuf.Push(numBlocks);
    reqBuf.Push64BitPointer(func);
    reqBuf.Push(blockSize);
    reqBuf.Push(dynamicSMemSize);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(numBlocks);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return cudaSuccess;

    //return func_entry(numBlocks, func, blockSize, dynamicSMemSize);
}

cudaError_t cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(
    int *numBlocks, const void *func, int blockSize, size_t dynamicSMemSize, unsigned int flags) {
    const char* func_name = "cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags";
    HookLog(func_name);
    using func_ptr = cudaError_t (*)(int *, const void *, int, size_t, unsigned int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    std::call_once(registerFlag, ClientEndpoint::SendRegisterRequest, clientEpObj, true);  // send the register requests first
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(int) + sizeof(size_t) + sizeof(unsigned int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR_WITH_FLAGS);
    // reqBuf.Push(numBlocks);
    reqBuf.Push64BitPointer(func);
    reqBuf.Push(blockSize);
    reqBuf.Push(dynamicSMemSize);
    reqBuf.Push(flags);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(numBlocks);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    tool::Logging(LOG_DEBUG, func_name, "numBlocks = %d\n", *numBlocks);
    return cudaSuccess;

    //return func_entry(numBlocks, func, blockSize, dynamicSMemSize);
}
#include "../../include/hook/hook.h"

inline int GetNcclTypeSize(ncclDataType_t type) {
    switch (type) {
        case ncclInt8:
        case ncclUint8:
            return 1;
        case ncclFloat16:
#if defined(__CUDA_BF16_TYPES_EXIST__)
        case ncclBfloat16:
#endif
            return 2;
        case ncclInt32:
        case ncclUint32:
        case ncclFloat32:
            return 4;
        case ncclInt64:
        case ncclUint64:
        case ncclFloat64:
            return 8;
        default:
            return -1;
    }
}

void* get_nccl_handle() {
    static void* handle = nullptr;
    if (!handle) {
        handle = dlopen("libnccl.so", RTLD_LAZY);
        if (!handle) {
            tool::Logging(LOG_ERROR, HOOK_LOG_TAG, "Failed to load 'libnccl.so': \n", dlerror());
            std::cerr << "Failed to load 'libnccl.so': " << dlerror() << std::endl;
        }
    }
    return handle;
}

ncclResult_t ncclMemAlloc(void** ptr, size_t size) {
    const char* func_name = "ncclMemAlloc";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(void* * , size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_MEM_ALLOC);
    reqBuf.Push(size);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = ptr;
    // res_iov.length = sizeof(uint64_t);
    // clientEpObj->RecvResponse(NCCL_MEM_ALLOC, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(ptr);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "allocated ptr = %p\n", *ptr, "\n");
    return ncclSuccess;

    // return func_entry(ptr, size);
}

ncclResult_t ncclMemFree(void *ptr) {
    const char* func_name = "ncclMemFree";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(void * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_MEM_FREE);
    reqBuf.Push64BitPointer(ptr);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(ptr);
}

ncclResult_t ncclGetVersion(int *version) {
    const char* func_name = "ncclGetVersion";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(int * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_GET_VERSION);
    int tmpDev = 0;
    reqBuf.Push(tmpDev); // dummy
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = version;
    // res_iov.length = sizeof(int);
    // clientEpObj->RecvResponse(NCCL_GET_VERSION, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(version);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return ncclSuccess;

    // return func_entry(version);
}

ncclResult_t ncclGetUniqueId(ncclUniqueId* uniqueId) {
    const char* func_name = "ncclGetUniqueId";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclUniqueId* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_GET_UNIQUE_ID);
    int tmpDev = 0;
    reqBuf.Push(tmpDev); // dummy
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = uniqueId;
    // res_iov.length = sizeof(ncclUniqueId);
    // clientEpObj->RecvResponse(NCCL_GET_UNIQUE_ID, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(uniqueId);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

#ifdef GV_GPUMAP
    gpuIdMap->UpdateUniqueID((uint8_t*)uniqueId, sizeof(ncclUniqueId));
    // ncclUniqueId tmpId;
    // gpuIdMap->RequestUniqueID((uint8_t*)&tmpId, sizeof(ncclUniqueId));
#endif

    return ncclSuccess;

    // return func_entry(uniqueId);
}

ncclResult_t ncclCommInitRankConfig(ncclComm_t* comm, int nranks, ncclUniqueId commId, int rank, ncclConfig_t* config) {
    const char* func_name = "ncclCommInitRankConfig";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t* , int, ncclUniqueId, int, ncclConfig_t* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_INIT_RANK_CONFIG);
    *comm = NULL;
    reqBuf.Push(comm);
    reqBuf.Push(nranks);
    reqBuf.Push(commId);
    reqBuf.Push(rank);
    reqBuf.Push(config);
    reqBuf.PushCString(config->netName);

    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = comm;
    // res_iov.length = sizeof(uint64_t);
    // clientEpObj->RecvResponse(NCCL_COMM_INIT_RANK_CONFIG, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(comm);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "allocated comm = %p", *comm, "\n");
    commDevIdx = myDevIdx;
    return ncclSuccess;

    // return func_entry(comm, nranks, commId, rank, config);
}

ncclResult_t ncclCommInitRank(ncclComm_t* comm, int nranks, ncclUniqueId commId, int rank) {
    const char* func_name = "ncclCommInitRank";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t* , int, ncclUniqueId, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_INIT_RANK);
    *comm = NULL;
    reqBuf.Push(comm);
    reqBuf.Push(nranks);
    reqBuf.Push(commId);
    reqBuf.Push(rank);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = comm;
    // res_iov.length = sizeof(uint64_t);
    // clientEpObj->RecvResponse(NCCL_COMM_INIT_RANK, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(comm);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "[pid:%d, tid:%d] allocated comm = %p\n", processID, threadID, *comm);
    commDevIdx = myDevIdx;
    return ncclSuccess;

    // return func_entry(comm, nranks, commId, rank);
}

ncclResult_t ncclCommInitAll(ncclComm_t* comm, int ndev, const int* devlist) { //todo: devlist needs to re-map
    const char* func_name = "ncclCommInitAll";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t* , int, const int* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_INIT_ALL);
    for (int i = 0; i < ndev; i++) {
        comm[i] = NULL;
    }
    reqBuf.Push((uint64_t*)comm, ndev);
    reqBuf.Push(ndev);
    reqBuf.PushConst(devlist, ndev);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = comm;
    // res_iov.length = sizeof(uint64_t) * ndev;
    // clientEpObj->RecvResponse(NCCL_COMM_INIT_ALL, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push((uint64_t*)comm, ndev);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "allocated comm for %d devices\n", ndev);
    return ncclSuccess;

    // return func_entry(comm, ndev, devlist);
}

ncclResult_t ncclCommFinalize(ncclComm_t comm) {
    const char* func_name = "ncclCommFinalize";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_FINALIZE);
    reqBuf.Push64BitPointer(comm);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(comm);
}

ncclResult_t ncclCommDestroy(ncclComm_t comm) {
    const char* func_name = "ncclCommDestroy";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_DESTROY);
    reqBuf.Push64BitPointer(comm);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(comm);
}

ncclResult_t ncclCommAbort(ncclComm_t comm) {
    const char* func_name = "ncclCommAbort";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_ABORT);
    reqBuf.Push64BitPointer(comm);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(comm);
}

ncclResult_t ncclCommSplit(ncclComm_t comm, int color, int key, ncclComm_t *newcomm, ncclConfig_t* config) {
    const char* func_name = "ncclCommSplit";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t, int, int, ncclComm_t * , ncclConfig_t* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_SPLIT);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push(color);
    reqBuf.Push(key);
    *newcomm = NULL;
    reqBuf.Push(newcomm);
    reqBuf.Push(config);
    reqBuf.PushCString(config->netName);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = newcomm;
    // res_iov.length = sizeof(uint64_t);
    // clientEpObj->RecvResponse(NCCL_COMM_INIT_RANK, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(newcomm);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    tool::Logging(LOG_DEBUG, func_name, "allocated comm = %p", *newcomm, "\n");
    return ncclSuccess;

    // return func_entry(comm, color, key, newcomm, config);
}

ncclResult_t ncclCommGetAsyncError(ncclComm_t comm, ncclResult_t *asyncError) { //todo: DDP
    const char* func_name = "ncclCommGetAsyncError";

    if (clientEpObj == nullptr) {
        myDevIdx = commDevIdx;
    } // new thread will call this function at the beginning

    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclComm_t, ncclResult_t * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_GET_ASYNC_ERROR);
    reqBuf.Push64BitPointer(comm);
    // // clientEpObj->SendRequest(&reqBuf);

    // // ucp_dt_iov_t res_iov;
    // // res_iov.buffer = asyncError;
    // // res_iov.length = sizeof(ncclResult_t);
    // // clientEpObj->RecvResponse(NCCL_COMM_GET_ASYNC_ERROR, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(asyncError);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    // *asyncError = ncclSuccess;
    return ncclSuccess;

    // return func_entry(comm, asyncError);
}

const char* ncclGetLastError(ncclComm_t comm) {
    const char* func_name = "ncclGetLastError";
    HookLog(func_name);
    using func_ptr = const char* (*)(ncclComm_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));
    return func_entry(comm);
}

const char*  ncclGetErrorString(ncclResult_t result) {
    const char* func_name = "ncclGetErrorString";
    HookLog(func_name);
    using func_ptr = const char* (*)(ncclResult_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));
    return func_entry(result);
}

ncclResult_t ncclCommCount(const ncclComm_t comm, int* count) {
    const char* func_name = "ncclCommCount";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const ncclComm_t, int* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_COUNT);
    reqBuf.Push64BitPointer(comm);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = count;
    // res_iov.length = sizeof(int);
    // clientEpObj->RecvResponse(NCCL_COMM_COUNT, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(count);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return ncclSuccess;

    // return func_entry(comm, count);
}

ncclResult_t ncclCommCuDevice(const ncclComm_t comm, int* device) {
    const char* func_name = "ncclCommCuDevice";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const ncclComm_t, int* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_CU_DEVICE);
    reqBuf.Push64BitPointer(comm);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = device;
    // res_iov.length = sizeof(int);
    // clientEpObj->RecvResponse(NCCL_COMM_CU_DEVICE, &res_iov, 1);
    int gpuIdInNode = 0;
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(gpuIdInNode); // todo: re-map the device id
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);  

#ifdef GV_GPUMAP
    if (gpuIdMap->GetGPUKey(gpuIdInNode, device) == false) {
        tool::Logging(LOG_ERROR, func_name, "failed to get the virtual device index for the GPU ID %d\n", gpuIdInNode);
        return ncclSystemError;
    }
#else
    *device = gpuIdInNode;
#endif

    return ncclSuccess;

    // return func_entry(comm, device);
}

ncclResult_t ncclCommUserRank(const ncclComm_t comm, int* rank) {
    const char* func_name = "ncclCommUserRank";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const ncclComm_t, int* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_USER_RANK);
    reqBuf.Push64BitPointer(comm);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = rank;
    // res_iov.length = sizeof(int);
    // clientEpObj->RecvResponse(NCCL_COMM_USER_RANK, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(rank);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return ncclSuccess;

    // return func_entry(comm, rank);
}

ncclResult_t ncclCommRegister(const ncclComm_t comm, void* buff, size_t size, void** handle) {
    const char* func_name = "ncclCommRegister";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const ncclComm_t, void* , size_t, void* * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_REGISTER);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(buff); // devptr
    reqBuf.Push(size);
    *handle = NULL;
    reqBuf.Push(handle);
    printf("comm: %p, buff: %p, size: %zu\n", comm, buff, size);
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = handle;
    // res_iov.length = sizeof(uint64_t);
    // clientEpObj->RecvResponse(NCCL_COMM_REGISTER, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(handle);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return ncclSuccess;

    // return func_entry(comm, buff, size, handle);
}

ncclResult_t ncclCommDeregister(const ncclComm_t comm, void* handle) {
    const char* func_name = "ncclCommDeregister";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const ncclComm_t, void* );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_COMM_DEREGISTER);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(handle);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(comm, handle);
}

ncclResult_t ncclRedOpCreatePreMulSum(ncclRedOp_t *op, void *scalar, ncclDataType_t datatype, ncclScalarResidence_t residence, ncclComm_t comm) {
    const char* func_name = "ncclRedOpCreatePreMulSum";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclRedOp_t * , void * , ncclDataType_t, ncclScalarResidence_t, ncclComm_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_RED_OP_CREATE_PRE_MUL_SUM);
    *op = ncclSum;
    reqBuf.Push(op);
    reqBuf.Push(datatype);
    reqBuf.Push(residence);
    reqBuf.Push64BitPointer(comm);
    if (residence == ncclScalarHostImmediate) {
        reqBuf.PushVar(scalar, GetNcclTypeSize(datatype));
    } else {
        reqBuf.Push64BitPointer(scalar);
    }
    // clientEpObj->SendRequest(&reqBuf);

    // ucp_dt_iov_t res_iov;
    // res_iov.buffer = op;
    // res_iov.length = sizeof(ncclRedOp_t);
    // clientEpObj->RecvResponse(NCCL_RED_OP_CREATE_PRE_MUL_SUM, &res_iov, 1);
    RequestIOV resBuf = RequestIOV();
    resBuf.Push(op);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return ncclSuccess;

    // return func_entry(op, scalar, datatype, residence, comm);
}

ncclResult_t ncclRedOpDestroy(ncclRedOp_t op, ncclComm_t comm) {
    const char* func_name = "ncclRedOpDestroy";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(ncclRedOp_t, ncclComm_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_RED_OP_DESTROY);
    reqBuf.Push(op);
    reqBuf.Push64BitPointer(comm);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(op, comm);
}

ncclResult_t ncclReduce(const void* sendbuff, void* recvbuff, size_t count, ncclDataType_t datatype, ncclRedOp_t op, int root, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclReduce";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const void* , void* , size_t, ncclDataType_t, ncclRedOp_t, int, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_REDUCE);
    reqBuf.Push64BitPointer(sendbuff); // devptr
    reqBuf.Push64BitPointer(recvbuff); // devptr
    reqBuf.Push(count);
    reqBuf.Push(datatype);
    reqBuf.Push(op);
    reqBuf.Push(root);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(sendbuff, recvbuff, count, datatype, op, root, comm, stream);
}

ncclResult_t ncclBroadcast(const void* sendbuff, void* recvbuff, size_t count, ncclDataType_t datatype, int root, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclBroadcast";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const void* , void* , size_t, ncclDataType_t, int, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_BROADCAST);
    reqBuf.Push64BitPointer(sendbuff); // devptr
    reqBuf.Push64BitPointer(recvbuff); // devptr
    reqBuf.Push(count);
    reqBuf.Push(datatype);
    reqBuf.Push(root);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(sendbuff, recvbuff, count, datatype, root, comm, stream);
}

ncclResult_t ncclAllReduce(const void* sendbuff, void* recvbuff, size_t count, ncclDataType_t datatype, ncclRedOp_t op, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclAllReduce";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const void* , void* , size_t, ncclDataType_t, ncclRedOp_t, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_ALL_REDUCE);
    reqBuf.Push64BitPointer(sendbuff); // devptr
    reqBuf.Push64BitPointer(recvbuff); // devptr
    reqBuf.Push(count);
    reqBuf.Push(datatype);
    reqBuf.Push(op);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(sendbuff, recvbuff, count, datatype, op, comm, stream);
}

ncclResult_t ncclReduceScatter(const void* sendbuff, void* recvbuff, size_t recvcount, ncclDataType_t datatype, ncclRedOp_t op, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclReduceScatter";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const void* , void* , size_t, ncclDataType_t, ncclRedOp_t, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_REDUCE_SCATTER);
    reqBuf.Push64BitPointer(sendbuff); // devptr
    reqBuf.Push64BitPointer(recvbuff); // devptr
    reqBuf.Push(recvcount);
    reqBuf.Push(datatype);
    reqBuf.Push(op);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(sendbuff, recvbuff, recvcount, datatype, op, comm, stream);
}

ncclResult_t ncclAllGather(const void* sendbuff, void* recvbuff, size_t sendcount, ncclDataType_t datatype, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclAllGather";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const void* , void* , size_t, ncclDataType_t, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_ALL_GATHER);
    reqBuf.Push64BitPointer(sendbuff); // devptr
    reqBuf.Push64BitPointer(recvbuff); // devptr
    reqBuf.Push(sendcount);
    reqBuf.Push(datatype);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(sendbuff, recvbuff, sendcount, datatype, comm, stream);
}

ncclResult_t ncclSend(const void* sendbuff, size_t count, ncclDataType_t datatype, int peer, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclSend";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(const void* , size_t, ncclDataType_t, int, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_SEND);
    reqBuf.Push64BitPointer(sendbuff); // devptr
    reqBuf.Push(count);
    reqBuf.Push(datatype);
    reqBuf.Push(peer);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(sendbuff, count, datatype, peer, comm, stream);
}

ncclResult_t ncclRecv(void* recvbuff, size_t count, ncclDataType_t datatype, int peer, ncclComm_t comm, cudaStream_t stream) {
    const char* func_name = "ncclRecv";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)(void* , size_t, ncclDataType_t, int, ncclComm_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_RECV);
    reqBuf.Push64BitPointer(recvbuff); // devptr
    reqBuf.Push(count);
    reqBuf.Push(datatype);
    reqBuf.Push(peer);
    reqBuf.Push64BitPointer(comm);
    reqBuf.Push64BitPointer(stream);
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry(recvbuff, count, datatype, peer, comm, stream);
}

ncclResult_t ncclGroupStart() {
    const char* func_name = "ncclGroupStart";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_GROUP_START);
    int tmpDev = 0;
    reqBuf.Push(tmpDev); // dummy
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry();
}

ncclResult_t ncclGroupEnd() {
    const char* func_name = "ncclGroupEnd";
    HookLog(func_name);
    using func_ptr = ncclResult_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nccl_handle(), func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(NCCL_GROUP_END);
    int tmpDev = 0;
    reqBuf.Push(tmpDev); // dummy
    clientEpObj->SendRequest(&reqBuf);
    return ncclSuccess;

    // return func_entry();
}


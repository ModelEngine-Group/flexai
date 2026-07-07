#include "../../include/serverEndpoint.h"

static const char* myName = "ncclHandle";

DEFINE_SERVER_AM_CALLBACK(ncclMemAllocHandle) {
    tool::Logging(myName, "NCCL_MEM_ALLOC\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    void*               devPtr      = NULL;
    size_t              cuda_size   = reqBuf.Pop<size_t>();
    ncclResult_t        exit_code   = ncclMemAlloc(&devPtr, cuda_size);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclMemAlloc success, devPtr = %p with size = %zu\n", devPtr, cuda_size);
        uint64_t        virtAddr    = (uint64_t)devPtr;
        
#ifdef GV_MEMORY
        virtAddr = serverEp->_cuInfoMap.blockManager->AddBlock((uint64_t)devPtr, cuda_size, true);
#else
        size_t blockNum = (
            serverEp->_cuInfoMap.blockInfoList.emplace_back(Block_t{.devPtr = (uint64_t)devPtr, .size = cuda_size, .valid = true}), 
            serverEp->_cuInfoMap.blockInfoList.size() - 1
        );
#endif // GV_MEMORY

#ifdef GV_MEMORY_PTX
        if (blockNum >= BLOCKS_MAX_NUM) {
            tool::Logging(LOG_ERROR, myName, "ncclMemAlloc failed: current blockNum(%zu) >= BLOCKS_MAX_NUM(%zu)\n", blockNum, BLOCKS_MAX_NUM);
            return UCS_ERR_IO_ERROR;
        }
        virtAddr = blockNum << BLOCK_SHIFT_BIT;
#endif // GV_MEMORY_PTX

        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_MEM_ALLOC);
        // resBuf.Push64BitPointer(devPtr);
        resBuf.Push(virtAddr);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclMemAlloc failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclMemFreeHandle) {
    tool::Logging(myName, "NCCL_MEM_FREE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virtAddr    = reqBuf.Pop<uint64_t>();
    void*               devPtr      = serverEp->GetDevPtr(virtAddr);
    ncclResult_t        exit_code   = ncclMemFree(devPtr);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclMemFree success, devPtr: %p\n", devPtr);
#ifdef GV_MEMORY_PTX
        size_t      blockIdx    = serverEp->GetBlockIdx(virtAddr);
        serverEp->_cuInfoMap.blockInfoList[blockIdx].valid = false;
#endif // GV_MEMORY
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclMemFree failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclGroupStartHandle) {
    tool::Logging(myName, "NCCL_GROUP_START\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclResult_t        exit_code   = ncclGroupStart();
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclGroupStart success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclGroupStart failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclGroupEndHandle) {
    tool::Logging(myName, "NCCL_GROUP_END\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclResult_t        exit_code   = ncclGroupEnd();
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclGroupEnd success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclGroupEnd failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommInitRankHandle) {
    tool::Logging(myName, "NCCL_COMM_INIT_RANK\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    int                 nranks      = reqBuf.Pop<int>();
    ncclUniqueId        commId      = reqBuf.Pop<ncclUniqueId>();
    int                 rank        = reqBuf.Pop<int>();
    ncclComm_t          comm        = NULL;
    ncclResult_t        exit_code   = ncclCommInitRank(&comm, nranks, commId, rank);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommInitRank success, comm: %p, nranks: %d, rank: %d\n", comm, nranks, rank);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, comm);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(NCCL_COMM_INIT_RANK);
            comm = (ncclComm_t)serverEp->GetHandleVirAddr(comm, NCCL_COMM_INIT_RANK);
            resBuf.Push64BitPointer(comm);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        serverEp->curComm = comm;
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommInitRank failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommInitAllHandle) {
    tool::Logging(myName, "NCCL_COMM_INIT_ALL\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t*           virAddr     = reqBuf.AssignAddr<uint64_t>();
    int                 ndev        = reqBuf.Pop<int>();
    const int*          devlist     = reqBuf.AssignAddr<const int>();
    ncclComm_t comms[ndev]; // todo: may be invalid
    ncclResult_t        exit_code   = ncclCommInitAll(comms, ndev, devlist);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommInitAll success, ndev: %d\n", ndev);
        if (virAddr[0] != 0) {
            for (int i = 0; i < ndev; i++) {
                serverEp->SetHandleVirAddr(virAddr[i], comms[i]);
            }
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(NCCL_COMM_INIT_ALL);
            for (int i = 0; i < ndev; i++) {
                comms[i] = (ncclComm_t)serverEp->GetHandleVirAddr(comms[i], NCCL_COMM_INIT_ALL);

            }
            resBuf.Push((uint64_t*)comms, ndev);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommInitAll failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommInitRankConfigHandle) {
    tool::Logging(myName, "NCCL_COMM_INIT_RANK_CONFIG\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    int                 nranks      = reqBuf.Pop<int>();
    ncclUniqueId        commId      = reqBuf.Pop<ncclUniqueId>();
    int                 rank        = reqBuf.Pop<int>();
    ncclConfig_t*       config      = reqBuf.AssignAddr<ncclConfig_t>();
    char*               netName     = reqBuf.AssignAddr<char>();
    config->netName = netName;
    ncclComm_t          comm        = NULL;
    ncclResult_t        exit_code   = ncclCommInitRankConfig(&comm, nranks, commId, rank, config);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommInitRankConfig success, comm: %p, nranks: %d, rank: %d\n", comm, nranks, rank);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, comm);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(NCCL_COMM_INIT_RANK_CONFIG);
            comm = (ncclComm_t)serverEp->GetHandleVirAddr(comm, NCCL_COMM_INIT_RANK_CONFIG);
            resBuf.Push64BitPointer(comm);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        serverEp->curComm = comm;
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommInitRankConfig failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommDestroyHandle) {
    tool::Logging(myName, "NCCL_COMM_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    ncclResult_t        exit_code   = ncclCommDestroy(comm);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommDestroy success, comm: %p\n", comm);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommDestroy failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommSplitHandle) {
    tool::Logging(myName, "NCCL_COMM_SPLIT\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                 color       = reqBuf.Pop<int>();
    int                 key         = reqBuf.Pop<int>();
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    ncclConfig_t*       config      = reqBuf.AssignAddr<ncclConfig_t>();
    char*               netName     = reqBuf.AssignAddr<char>();
    config->netName = netName;
    ncclComm_t          newcomm     = NULL;
    ncclResult_t        exit_code   = ncclCommSplit(comm, color, key, &newcomm, config);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommSplit success, oldcomm: %p, newcomm: %p\n", comm, newcomm);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, newcomm);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(NCCL_COMM_SPLIT);
            newcomm = (ncclComm_t)serverEp->GetHandleVirAddr(newcomm, NCCL_COMM_SPLIT);
            resBuf.Push64BitPointer(newcomm);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommSplit failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommFinalizeHandle) {
    tool::Logging(myName, "NCCL_COMM_FINALIZE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    ncclResult_t        exit_code   = ncclCommFinalize(comm);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommFinalize success, comm: %p\n", comm);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommFinalize failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommGetAsyncErrorHandle) {
    tool::Logging(myName, "NCCL_COMM_GET_ASYNC_ERROR\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    ncclResult_t        asyncError  = ncclSuccess;
    ncclResult_t        exit_code   = ncclCommGetAsyncError(comm, &asyncError);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommGetAsyncError success, asyncError: %d\n", asyncError);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_COMM_GET_ASYNC_ERROR);
        resBuf.Push(asyncError);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommGetAsyncError failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommCountHandle) {
    tool::Logging(myName, "NCCL_COMM_COUNT\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                 count       = 0;
    ncclResult_t        exit_code   = ncclCommCount(comm, &count);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommCount success, count: %d\n", count);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_COMM_COUNT);
        resBuf.Push(count);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommCount failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommUserRankHandle) {
    tool::Logging(myName, "NCCL_COMM_USER_RANK\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                 rank        = 0;
    ncclResult_t        exit_code   = ncclCommUserRank(comm, &rank);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommUserRank success, rank: %d\n", rank);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_COMM_USER_RANK);
        resBuf.Push(rank);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommUserRank failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommCuDeviceHandle) { 
    tool::Logging(myName, "NCCL_COMM_CU_DEVICE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                 device      = 0;
    ncclResult_t        exit_code   = ncclCommCuDevice(comm, &device);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommCuDevice success, device: %d\n", device);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_COMM_CU_DEVICE);
        resBuf.Push(device);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommCuDevice failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommAbortHandle) {
    tool::Logging(myName, "NCCL_COMM_ABORT\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    ncclResult_t        exit_code   = ncclCommAbort(comm);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommAbort success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommAbort failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommRegisterHandle) {
    tool::Logging(myName, "NCCL_COMM_REGISTER\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const ncclComm_t    comm        = (const ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*               buff        = (void*)reqBuf.Pop<uint64_t>();
    size_t              size        = reqBuf.Pop<size_t>();
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    void*               handle      = NULL;
    buff = serverEp->GetDevPtr((uint64_t)buff);
    ncclResult_t        exit_code   = ncclCommRegister(comm, buff, size, &handle);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommRegister success, buff: %p, handle: %p\n", buff, handle);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, handle);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(NCCL_COMM_REGISTER);
            handle = serverEp->GetHandleVirAddr(handle, NCCL_COMM_REGISTER);
            resBuf.Push64BitPointer(handle);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommRegister failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclCommDeregisterHandle) {
    tool::Logging(myName, "NCCL_COMM_DEREGISTER\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const ncclComm_t    comm        = (const ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*               handle      = (void*)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    ncclResult_t        exit_code   = ncclCommDeregister(comm, handle);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclCommDeregister success, handle: %p\n", handle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclCommDeregister failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclGetUniqueIdHandle) {
    tool::Logging(myName, "NCCL_GET_UNIQUE_ID\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclUniqueId        uniqueId;
    ncclResult_t        exit_code   = ncclGetUniqueId(&uniqueId);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclGetUniqueId success\n");
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_GET_UNIQUE_ID);
        resBuf.Push(uniqueId);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclGetUniqueId failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclGetVersionHandle) {
    tool::Logging(myName, "NCCL_GET_VERSION\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    int                 version;
    ncclResult_t        exit_code   = ncclGetVersion(&version);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclGetVersion success, version: %d\n", version);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(NCCL_GET_VERSION);
        resBuf.Push(version);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclGetVersion failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }

}

DEFINE_SERVER_AM_CALLBACK(ncclAllReduceHandle) {
    tool::Logging(myName, "NCCL_ALL_REDUCE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const void*         sendbuff    = (const void*) reqBuf.Pop<uint64_t>();
    void*               recvbuff    = (void*)       reqBuf.Pop<uint64_t>();
    size_t              count       = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    ncclRedOp_t         op          = reqBuf.Pop<ncclRedOp_t>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    sendbuff = serverEp->GetDevPtr((uint64_t)sendbuff);
    recvbuff = serverEp->GetDevPtr((uint64_t)recvbuff);

    // // debug:
    // if (serverEp->ckptCnt >= 2) {
    //     ncclCommAbort(comm);
    //     ncclCommDestroy(comm);
    //     {
    //         boost::unique_lock<boost::mutex> lock(serverEp->backupSync_.mutex);
    //         serverEp->backupSync_.cv.wait(lock, [serverEp] { return !serverEp->bufferReady_; });
    //     }
    //     sleep(3); // wait for the client to finish shrinking of api list
    //     exit(EXIT_FAILURE);
    //     // serverEp->connStatus_.isClosed = true;
    //     // for (int i = 0; i < 3; i++) serverEp->shmQueSizes_[i]->SaveState();
    //     // return UCS_ERR_CONNECTION_RESET;
    // }

    ncclResult_t        exit_code   = ncclAllReduce(sendbuff, recvbuff, count, datatype, op, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclAllReduce success, sendbuff: %p, recvbuff: %p, count: %d, stream: %p\n", sendbuff, recvbuff, count, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclAllReduce failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclReduceHandle) {
    tool::Logging(myName, "NCCL_REDUCE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const void*         sendbuff    = (const void*) reqBuf.Pop<uint64_t>();
    void*               recvbuff    = (void*)       reqBuf.Pop<uint64_t>();
    size_t              count       = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    ncclRedOp_t         op          = reqBuf.Pop<ncclRedOp_t>();
    int                 root        = reqBuf.Pop<int>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    sendbuff = serverEp->GetDevPtr((uint64_t)sendbuff);
    recvbuff = serverEp->GetDevPtr((uint64_t)recvbuff);
    ncclResult_t        exit_code   = ncclReduce(sendbuff, recvbuff, count, datatype, op, root, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclReduce success, sendbuff: %p, recvbuff: %p, count: %d, stream: %p\n", sendbuff, recvbuff, count, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclReduce failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclReduceScatterHandle) {
    tool::Logging(myName, "NCCL_REDUCE_SCATTER\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const void*         sendbuff    = (const void*) reqBuf.Pop<uint64_t>();
    void*               recvbuff    = (void*)       reqBuf.Pop<uint64_t>();
    size_t              recvcount   = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    ncclRedOp_t         op          = reqBuf.Pop<ncclRedOp_t>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    sendbuff = serverEp->GetDevPtr((uint64_t)sendbuff);
    recvbuff = serverEp->GetDevPtr((uint64_t)recvbuff);
    ncclResult_t        exit_code   = ncclReduceScatter(sendbuff, recvbuff, recvcount, datatype, op, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclReduceScatter success, sendbuff: %p, recvbuff: %p, recvcount: %d, stream: %p\n", sendbuff, recvbuff, recvcount, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclReduceScatter failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclAllGatherHandle) {
    tool::Logging(myName, "NCCL_ALL_GATHER\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const void*         sendbuff    = (const void*) reqBuf.Pop<uint64_t>();
    void*               recvbuff    = (void*)       reqBuf.Pop<uint64_t>();
    size_t              sendcount   = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    sendbuff = serverEp->GetDevPtr((uint64_t)sendbuff);
    recvbuff = serverEp->GetDevPtr((uint64_t)recvbuff);
    ncclResult_t        exit_code   = ncclAllGather(sendbuff, recvbuff, sendcount, datatype, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclAllGather success, sendbuff: %p, recvbuff: %p, sendcount: %d, stream: %p\n", sendbuff, recvbuff, sendcount, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclAllGather failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclBroadcastHandle) {
    tool::Logging(myName, "NCCL_BROADCAST\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const void*         sendbuff    = (const void*) reqBuf.Pop<uint64_t>();
    void*               recvbuff    = (void*)       reqBuf.Pop<uint64_t>();
    size_t              count       = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    int                 root        = reqBuf.Pop<int>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    sendbuff = serverEp->GetDevPtr((uint64_t)sendbuff);
    recvbuff = serverEp->GetDevPtr((uint64_t)recvbuff);
    ncclResult_t        exit_code   = ncclBroadcast(sendbuff, recvbuff, count, datatype, root, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclBroadcast success, sendbuff: %p, recvbuff: %p, count: %d, stream: %p\n", sendbuff, recvbuff, count, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclBroadcast failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }    
}

DEFINE_SERVER_AM_CALLBACK(ncclSendHandle) {
    tool::Logging(myName, "NCCL_SEND\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    const void*         sendbuff    = (const void*) reqBuf.Pop<uint64_t>();
    size_t              count       = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    int                 peer        = reqBuf.Pop<int>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    sendbuff = serverEp->GetDevPtr((uint64_t)sendbuff);
    ncclResult_t        exit_code   = ncclSend(sendbuff, count, datatype, peer, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclSend success, sendbuff: %p, count: %d, stream: %p\n", sendbuff, count, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclSend failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclRecvHandle) {
    tool::Logging(myName, "NCCL_RECV\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    void*               recvbuff    = (void*)       reqBuf.Pop<uint64_t>();
    size_t              count       = reqBuf.Pop<size_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    int                 peer        = reqBuf.Pop<int>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    recvbuff = serverEp->GetDevPtr((uint64_t)recvbuff);
    ncclResult_t        exit_code   = ncclRecv(recvbuff, count, datatype, peer, comm, stream);
    if (exit_code == ncclSuccess) {
        serverEp->UpdateStream(stream);
        tool::Logging(myName, "ncclRecv success, recvbuff: %p, count: %d, stream: %p\n", recvbuff, count, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclRecv failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclRedOpCreatePreMulSumHandle) {
    tool::Logging(myName, "NCCL_RED_OP_CREATE_PRE_MUL_SUM\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclRedOp_t         virOp       = reqBuf.Pop<ncclRedOp_t>();
    ncclDataType_t      datatype    = reqBuf.Pop<ncclDataType_t>();
    ncclScalarResidence_t residence = reqBuf.Pop<ncclScalarResidence_t>();
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*               scalar      = NULL;
    ncclRedOp_t         op;
    if (residence == ncclScalarHostImmediate) {
        scalar = reqBuf.AssignAddr<void>();
    } else {
        scalar = (void*)reqBuf.Pop<uint64_t>();
        scalar = serverEp->GetDevPtr((uint64_t)scalar);
    }
    ncclResult_t        exit_code   = ncclRedOpCreatePreMulSum(&op, scalar, datatype, residence, comm);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclRedOpCreatePreMulSum success, scalar: %p, op: %d\n", scalar, op);
        if (virOp != 0) {
            serverEp->SetNcclRedOpVirAddr(virOp, op);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(NCCL_RED_OP_CREATE_PRE_MUL_SUM);
            op = serverEp->GetNcclRedOpVirAddr(op);
            resBuf.Push(op);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclRedOpCreatePreMulSum failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(ncclRedOpDestroyHandle) {
    tool::Logging(myName, "NCCL_RED_OP_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    ncclRedOp_t         op          = serverEp->GetNcclRedOp(reqBuf.Pop<ncclRedOp_t>(), true);
    ncclComm_t          comm        = (ncclComm_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    ncclResult_t        exit_code   = ncclRedOpDestroy(op, comm);
    if (exit_code == ncclSuccess) {
        tool::Logging(myName, "ncclRedOpDestroy success, op: %d\n", op);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "ncclRedOpDestroy failed: %s\n", ncclGetErrorString(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}
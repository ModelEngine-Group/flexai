#include "../../include/serverEndpoint.h"

static const char* myName = "CUDARuntimeMemoryHandle";

DEFINE_SERVER_AM_CALLBACK(cudaMallocHandle){
    tool::Logging(myName, "CUDA_MALLOC\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    size_t          cuda_size   = reqBuf.Pop<size_t>();
    bool            essential   = reqBuf.Pop<bool>();

    void*           devPtr      = NULL;
    cudaError_t     exit_code   = cudaMalloc(&devPtr, cuda_size);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaMalloc success, devPtr = %p with size = %d\n", devPtr, cuda_size);
        uint64_t virtAddr = (uint64_t)devPtr;
        
#ifdef GV_MEMORY
        virtAddr = serverEp->_cuInfoMap.blockManager->AddBlock((uint64_t)devPtr, cuda_size, essential);
#else 
        size_t blockNum = (
            serverEp->_cuInfoMap.blockInfoList.emplace_back(Block_t{.devPtr = (uint64_t)devPtr, .size = cuda_size, .valid = true}), 
            serverEp->_cuInfoMap.blockInfoList.size() - 1
        );
#endif // GV_MEMORY

#ifdef GV_MEMORY_PTX
        if (blockNum >= BLOCKS_MAX_NUM) {
            tool::Logging(LOG_ERROR, myName, "cudaMalloc failed: current blockNum(%zu) >= BLOCKS_MAX_NUM(%zu)\n", blockNum, BLOCKS_MAX_NUM);
            return UCS_ERR_IO_ERROR;
        }
        virtAddr = blockNum << BLOCK_SHIFT_BIT;
#endif // GV_MEMORY_PTX

#ifdef GV_Scheduler
        serverEp->sche->cal_job_mem(cuda_size);
#endif // GV_Scheduler

        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_MALLOC);
        // resBuf.Push64BitPointer(devPtr);
        resBuf.Push(virtAddr);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMalloc failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemsetHandle) {
    tool::Logging(myName, "CUDA_MEMSET\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    void*           devPtr      = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    // void*           devPtr      = (void*)reqBuf->Pop<uint64_t>();
    int             value       = reqBuf.Pop<int>();
    size_t          count       = reqBuf.Pop<size_t>();

    cudaError_t     exit_code   = cudaMemset(devPtr, value, count);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaMemset success, devPtr = %p(%llu) with value = %d and count = %d\n", devPtr, devPtr, value, count);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMemset failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemsetAsyncHandle) {
    tool::Logging(myName, "CUDA_MEMSET_ASYNC\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    void*           devPtr      = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    // void*           devPtr      = (void*)reqBuf->Pop<uint64_t>();
    int             value       = reqBuf.Pop<int>();
    size_t          count       = reqBuf.Pop<size_t>();
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;

    cudaError_t     exit_code   = cudaMemsetAsync(devPtr, value, count, stream);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaMemset success, devPtr = %p(%llu) with value = %d , count = %d , streamPtr=%p\n", devPtr, devPtr, value, count, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMemset failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemGetInfoHandle) {
    tool::Logging(myName, "CUDA_MEM_GET_INFO\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             replayDone  = reqBuf.Pop<int>();
    size_t          free        = 0;
    size_t          total       = 0;
    cudaError       exit_code   = cudaMemGetInfo(&free,&total);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaMemGetInfo success, free = %lu, total= %lu\n",free,total);
        if (replayDone) {
            serverEp->recoveryFlag_ = false;
            serverEp->_cuInfoMap.handleManager->Shrink();
            tool::Logging(LOG_INFO, "Backup", "replay done.\n");
        }
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_MEM_GET_INFO);
        resBuf.Push(free);
        resBuf.Push(total);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMemGetInfo failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaFreeHandle) {
    tool::Logging(myName, "CUDA_FREE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        virtAddr    = reqBuf.Pop<uint64_t>();
    void*           devPtr      = serverEp->GetDevPtr(virtAddr);

    cudaError_t     exit_code   = cudaFree(devPtr);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaFree success, devPtr = %p\n", devPtr);
#ifdef GV_MEMORY_PTX
        size_t      blockIdx    = serverEp->GetBlockIdx(virtAddr);
        serverEp->_cuInfoMap.blockInfoList[blockIdx].valid = false;
#endif // GV_MEMORY_PTX

#ifdef GV_MEMORY
        serverEp->_cuInfoMap.blockManager->ResetBlock(virtAddr);
#endif // GV_MEMORY

        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaFree failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyH2DHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_H2D\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    cudaMemcpyKind  kind        = cudaMemcpyHostToDevice;
    size_t          count       = 0;
    void*           dst         = NULL;
    ucs_status_t    status      = UCS_OK;
    uint8_t*        headers     = (uint8_t*)header; // header contains parameters for cudaMemcpy H2D
    memcpy(&kind,  headers, sizeof(cudaMemcpyKind));
    memcpy(&count, headers + sizeof(cudaMemcpyKind), sizeof(size_t));
    memcpy(&dst,   headers + sizeof(cudaMemcpyKind) + sizeof(size_t), sizeof(uint64_t));
    dst = serverEp->GetDevPtr((uint64_t)dst);

    if (kind != cudaMemcpyHostToDevice || count != length) {
        tool::Logging(LOG_ERROR, myName, "cudaMemcpy H2D failed: invalid cudaMemcpyKind(%d) or count(%zu)\n", kind, count);
        return UCS_ERR_IO_ERROR;
    }
    if (param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) {
        tool::Logging(myName, "cudaMemcpyH2D: RNDV request.\n");
        Request_t recv_request_ctx = {.type = 1, .complete = 0};
        ucp_request_param_t param2;
        param2.op_attr_mask     = UCP_OP_ATTR_FIELD_CALLBACK |
                                  UCP_OP_ATTR_FIELD_DATATYPE |
                                  UCP_OP_ATTR_FIELD_USER_DATA|
                                  UCP_OP_ATTR_FIELD_MEMORY_TYPE;
        param2.op_attr_mask    |= UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
        param2.datatype         = ucp_dt_make_contig(1);
        param2.user_data        = &recv_request_ctx;
        param2.cb.recv_am       = (ucp_am_recv_data_nbx_callback_t)RecvCallBack;
        param2.memory_type      = UCS_MEMORY_TYPE_CUDA;
        Request_t* rndv_request = (Request_t*)ucp_am_recv_data_nbx(serverEp->_dataWorker, data, dst, count, &param2);
        status = Wait(rndv_request, &recv_request_ctx, &serverEp->_dataWorker);
        if (status == UCS_OK) {
            tool::Logging(myName, "cudaMemcpy H2D: Host -> Device(%p) with %zu B\n", dst, count);
        }
        else {
            tool::Logging(myName, "cudaMemcpy H2D failed: ucp_am_recv_data_nbx(%s)\n", ucs_status_string(status));
        }
        ucp_request_free(rndv_request);
        return status;
    }
    else {
        tool::Logging(myName, "cudaMemcpy H2D: Eager request.\n");
        cudaError_t exit_code = cudaMemcpy(dst, data, count, kind);
        if (exit_code == cudaSuccess) {
            tool::Logging(myName, "cudaMemcpy H2D: Host -> Device(%p) with %zu B\n", dst, count);
            return UCS_OK;
        }
        else {
            tool::Logging(LOG_ERROR, myName, "cudaMemcpy H2D failed: %s\n", cudaGetErrorName(exit_code));
            return UCS_ERR_IO_ERROR;
        }
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyD2HHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_D2H\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaMemcpyKind  kind        = reqBuf.Pop<cudaMemcpyKind>();
    size_t          count       = reqBuf.Pop<size_t>();
    void*           src         = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    
    RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
    resBuf.PushRequestType(CUDA_MEMCPY_D2H);
    resBuf.Push((uint8_t*)src, count);
    serverEp->SendResponse(&resBuf, &param->reply_ep, UCS_MEMORY_TYPE_CUDA);

    tool::Logging(myName, "cudaMemcpy D2H: Device(%p) -> Host with %zu B\n", src, count);
    return UCS_OK;
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyD2DHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_D2D\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaMemcpyKind  kind        = reqBuf.Pop<cudaMemcpyKind>();
    size_t          count       = reqBuf.Pop<size_t>();
    void*           src         = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    void*           dst         = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());

    cudaError_t     exit_code   = cudaMemcpy(dst, src, count, kind);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaMemcpy D2D: Device(%p) -> Device(%p) with %zu B\n", src, dst, count);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMemcpy D2D failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyAsyncH2DHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_ASYNC_H2D\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    cudaMemcpyKind  kind        = cudaMemcpyHostToDevice;
    size_t          count       = 0;
    cudaStream_t    stream      = NULL;
    uint8_t         CurType     = MEMCPY_OTHER;
    void*           dst         = NULL;
    ucs_status_t    status      = UCS_OK;
    uint8_t*        headers     = (uint8_t*)header; // header contains parameters for cudaMemcpyAsync H2D
    size_t          headerOffset= 0;
    memcpy(&kind,  headers + headerOffset, sizeof(cudaMemcpyKind));
    headerOffset += sizeof(cudaMemcpyKind);
    memcpy(&count, headers + headerOffset, sizeof(size_t));
    headerOffset += sizeof(size_t);
    memcpy(&stream,headers + headerOffset, sizeof(uint64_t));
    stream = (cudaStream_t)serverEp->GetHandle((uint64_t)stream);
    headerOffset += sizeof(uint64_t);
    memcpy(&CurType,headers + headerOffset, sizeof(uint8_t));
    headerOffset += sizeof(uint8_t);
    memcpy(&dst,  headers + headerOffset, sizeof(uint64_t));
    headerOffset += sizeof(uint64_t);
    dst = serverEp->GetDevPtr((uint64_t)dst);

    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;

    if (kind != cudaMemcpyHostToDevice) {
        tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync H2D failed: invalid cudaMemcpyKind(%d)\n", kind);
        return UCS_ERR_IO_ERROR;
    }

    if (CurType != MEMCPY_OTHER) { // with data preloading optimization, data is already in shared memory queue
        size_t      shmQueueSize= 0;
        size_t      shmQueueIden= 0;
        if (serverEp->shmQueues_[CurType - 1] == NULL) {
            if (serverEp->shmQueSizes_[CurType - 1]->ReadCurSize(&shmQueueSize) == false || shmQueueSize == 0) {
                tool::Logging(LOG_ERROR, myName, "failed: read shared memory queue size failed\n");
                // return UCS_ERR_IO_ERROR;
                exit(EXIT_FAILURE);
            }
#ifdef GV_Scheduler
            if(serverEp->shmQueSizes_[CurType - 1]->ReadCurNumIt(&(serverEp->numIterations)) == false){
                tool::Logging(LOG_ERROR, myName, "failed: read numIterations failed\n", kind);
                return UCS_ERR_IO_ERROR;
            }
            serverEp->sche->get_Iteration(serverEp->numIterations);
            tool::Logging(LOG_INFO, myName, "numIterations=%d\n", serverEp->numIterations);
#endif
            if (serverEp->shmQueSizes_[CurType - 1]->ReadCurIdent(&shmQueueIden) == false) {
                tool::Logging(LOG_ERROR, myName, "failed: read shared memory queue identify prefix failed\n");
                // return UCS_ERR_IO_ERROR;
                exit(EXIT_FAILURE);
            }
            tool::Logging(myName, "cudaMemcpyAsync H2D: shmQueueSize=%zu, shmQueueIdentifyPrefix=%zu\n", shmQueueSize, shmQueueIden);
            serverEp->shmQueues_[CurType - 1] = CMessageQueue::GetInstance(GENERATE_KEY(shmQueueIden, CurType), shmQueueSize, eQueueModel::ONE_READ_ONE_WRITE);
        }
        if (serverEp->lastCopyLen_[CurType] != 0 && serverEp->lastCopyType_ == CurType) { // avoid pop the empty queue
            serverEp->shmQueues_[CurType - 1]->Pop(serverEp->lastCopyLen_[CurType]);
        }
        uint8_t*    src         = NULL;
        size_t      len         = 0;
        int         error_code  = serverEp->shmQueues_[CurType - 1]->ReadMessage(&src, &len); //todo
        tool::Logging(myName,"ZWX: kind=%d CurType=%02x error_code=%d len=%zu count=%zu last_copyLen=%zu dst=%p\n", kind, CurType, error_code, len, count, serverEp->lastCopyLen_[CurType], dst);
        while(error_code != 0){ // blocking read until data is ready in shared memory queue
            // tool::Logging(myName, "HYF# repeated reading");
            // tool::Logging(myName, "read failed from shared memory queue: %zu\n", len);
            error_code = serverEp->shmQueues_[CurType - 1]->ReadMessage(&src, &len);
        }
        if (len == count) {
            tool::Logging(myName, "cudaMemcpyAsync H2D: read data (%zu B) from shm queue(type=%02x)\n", len, CurType);
            // printf("data[0-7]: ");
            // for (size_t i = 0; i < 8; i++) {
            //     printf("%02x ", src[i]);
            // }
            // printf(", data[%zu-%zu]: ", len - 8, len - 1);
            // for (size_t i = len - 8; i < len; i++) {
            //     printf("%02x ", src[i]);
            // }
            // printf("\n");

            cudaError_t exit_code = cudaMemcpyAsync(dst, src, len, kind, stream);
            if (exit_code == cudaSuccess) {
                //cudaStreamSynchronize(stream); // make sure the data has been copied completely
                serverEp->lastCopyLen_[CurType]  = len;
                serverEp->lastCopyType_          = CurType;
                tool::Logging(myName, "cudaMemcpyAsync H2D: Host(%p) -> Device(%p) with %zu B\n", src, dst, count);
#ifdef GV_Scheduler
                ++serverEp->numIterations;
                if((serverEp->numIterations & 1) == 0) {
                    serverEp->sche->cal_add_It(serverEp->clientID_);
                }
#endif
                // return UCS_OK;
            }
            else {
                tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync H2D failed: %s\n", cudaGetErrorName(exit_code));
                return UCS_ERR_IO_ERROR;
            }
        }
        else {
            tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync H2D failed: read failed from shm queue, len(%zu B) != cnt(%zu B)\n", len, count);
            return UCS_ERR_IO_ERROR;
        }
    }
    else { // without data preloading, data transfer from remote host to current device
        if (count != length) {
            tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync H2D failed: invalid count(%zu), recv length(%zu)\n", count, length);
            return UCS_ERR_IO_ERROR;
        }

        if (param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) {
            tool::Logging(myName, "cudaMemcpyAsync H2D: RNDV request.\n");
            Request_t recv_request_ctx = {.type = 1, .complete = 0};
            ucp_request_param_t param2;
            param2.op_attr_mask     = UCP_OP_ATTR_FIELD_CALLBACK |
                                    UCP_OP_ATTR_FIELD_DATATYPE |
                                    UCP_OP_ATTR_FIELD_USER_DATA|
                                    UCP_OP_ATTR_FIELD_MEMORY_TYPE;
            param2.op_attr_mask    |= UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
            param2.datatype         = ucp_dt_make_contig(1);
            param2.user_data        = &recv_request_ctx;
            param2.cb.recv_am       = (ucp_am_recv_data_nbx_callback_t)RecvCallBack;
            param2.memory_type      = UCS_MEMORY_TYPE_CUDA;
            Request_t* rndv_request = (Request_t*)ucp_am_recv_data_nbx(serverEp->_dataWorker, data, dst, count, &param2);
            status = Wait(rndv_request, &recv_request_ctx, &serverEp->_dataWorker);
            ucp_request_free(rndv_request);
            if (status == UCS_OK) {
                tool::Logging(myName, "cudaMemcpyAsync H2D: Host -> Device(%p) with %zu B\n", dst, count);
            }
            else {
                tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync H2D failed: ucp_am_recv_data_nbx(%s)\n", ucs_status_string(status));
                return status;
            }
            // return status;
        }
        else {
            tool::Logging(myName, "cudaMemcpyH2D: Eager request.\n");
            cudaError_t exit_code = cudaMemcpyAsync(dst, data, count, kind, stream);
            if (exit_code == cudaSuccess) {
                tool::Logging(myName, "cudaMemcpyAsync H2D: Host -> Device(%p) with %zu B\n", dst, count);
                // return UCS_OK;
            }
            else {
                tool::Logging(LOG_ERROR, myName, "cudaMemcpyH2D failed: %s\n", cudaGetErrorName(exit_code));
                return UCS_ERR_IO_ERROR;
            }
        }
    }
    return UCS_OK;
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyAsyncD2HHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_ASYNC_D2H\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaMemcpyKind  kind        = reqBuf.Pop<cudaMemcpyKind>();
    size_t          count       = reqBuf.Pop<size_t>();
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*           src         = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    
    RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
    resBuf.PushRequestType(CUDA_MEMCPY_ASYNC_D2H);
    resBuf.Push((uint8_t*)src, count);
    serverEp->SendResponse(&resBuf, &param->reply_ep, UCS_MEMORY_TYPE_CUDA);

    tool::Logging(myName, "cudaMemcpyAsync D2H: Device(%p) -> Host with %zu B\n", src, count);
    return UCS_OK;
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyAsyncD2DHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_ASYNC_D2D\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaMemcpyKind  kind        = reqBuf.Pop<cudaMemcpyKind>();
    size_t          count       = reqBuf.Pop<size_t>();
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*           src         = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    void*           dst         = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    cudaError_t    exit_code   = cudaMemcpyAsync(dst, src, count, kind, stream);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaMemcpyAsync D2D: Device(%p) -> Device(%p) with %zu B\n", src, dst, count);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync D2D failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaMemcpyToSymbolHandle) {
    tool::Logging(myName, "CUDA_MEMCPY_TO_SYMBOL\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaMemcpyKind  kind        = reqBuf.Pop<cudaMemcpyKind>();
    size_t          count       = reqBuf.Pop<size_t>();
    size_t          offset      = reqBuf.Pop<size_t>();

    if (kind == cudaMemcpyHostToDevice || kind == cudaMemcpyDefault) {
        uint64_t    symbol      = reqBuf.Pop<uint64_t>();
        void*       src         = reqBuf.AssignAddrForAll<void*>();

        
        auto        it          = serverEp->_cuInfoMap.mapHostVar2CuDevPtr->find(symbol);
        if (it == serverEp->_cuInfoMap.mapHostVar2CuDevPtr->end()) { 
            tool::Logging(LOG_ERROR, myName, "cudaMemcpyToSymbolHandle failed: unknown symbol(%p)\n", symbol);
            return UCS_ERR_IO_ERROR;
        }
        void* cuDevPtr    = (void*)it->second;
        // cudaError_t exit_code   = cudaMemcpyToSymbol((void*)cuDevPtr, src, count, offset, kind);
        cudaError_t exit_code   = cudaMemcpy((uint8_t*)cuDevPtr + offset, src, count, kind);
        if (exit_code == cudaSuccess) {
            tool::Logging(myName, "cudaMemcpyToSymbol success\n");
            return UCS_OK;
        }
        else {
            tool::Logging(myName, "cudaMemcpyToSymbol failed: %s\n", cudaGetErrorName(exit_code));
            return UCS_ERR_IO_ERROR;
        }
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaMemcpyToSymbol failed: Unknown cudaMemcpyKind: %d\n", kind);
        return UCS_ERR_IO_ERROR;
    }
}



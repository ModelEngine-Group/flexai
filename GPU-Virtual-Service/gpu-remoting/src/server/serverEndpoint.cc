#include "../../include/serverEndpoint.h"

ServerEndpoint::ServerEndpoint(ucp_worker_h dataWorker, UCPConnection_t conn){
    _dataWorker = dataWorker;
    _connectReq = conn.conn_request;
    clientID_ = conn.client_id;
    clientIP_ = conn.client_ip;
    clientPort_ = conn.client_port;
    string wholeName = myShortName_ + std::string(" ") + clientIP_ + std::string(":") + clientPort_;
    myName_ = strdup(wholeName.c_str());

    _cuInfoMap.isFirstIter = true;
    _cuInfoMap.mapFatBinHandle2CuModule = new robin_hood::unordered_flat_map<uint64_t, CUmodule>();
    _cuInfoMap.mapHost2CuFunc = new robin_hood::unordered_flat_map<uint64_t, CUfunction>();
    _cuInfoMap.mapDevName2DevPtr = new robin_hood::unordered_flat_map<std::string, uint64_t>();
    _cuInfoMap.mapHostVar2CuDevPtr = new robin_hood::unordered_flat_map<uint64_t, CUdeviceptr>();

#ifdef GV_HANDLE
    _cuInfoMap.handleManager = new HandleMapper();
#endif // GV_HANDLE

#ifdef GV_MEMORY
    _cuInfoMap.blockManager = new DeviceBlockMapper();
#else
    _cuInfoMap.blockInfoList.reserve(1000);
    _cuInfoMap.blockInfoList.emplace_back(Block_t{.devPtr = (uint64_t)NULL, .size = 0, .valid = true});                     // let the blockNum start from 1
#endif // GV_MEMORY

    // _cuInfoMap.ncclUidList.reserve(5);
    // _cuInfoMap.ncclUidList.emplace_back(ncclUniqueId{});                                                                    // let the ncclUidNum start from 1
    _cuInfoMap.ncclRedOpList.resize(ncclNumOps + 1, static_cast<ncclRedOp_t>(0));
    for (int i = 0; i < ncclNumOps; i++) {
        _cuInfoMap.ncclRedOpList[i] = static_cast<ncclRedOp_t>(i);
    }


    /* __cudaRegister will initialize shmQueSizes_, imageQueue and ptxExtractor */
    _cuInfoMap.imageQueue = nullptr; 
    _cuInfoMap.ptxExtractor = nullptr;
    _fatbinList.reserve(240);

    trainTensors_.reserve(5);
    boost::thread_attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
#ifdef GV_BACKUP
    stateBackup_.Start(boost::bind(&ServerEndpoint::Backup2Storage, this));
    // backupStorageThread_ = new boost::thread(attrs, boost::bind(&ServerEndpoint::Backup2Storage, this));
    eventWatchedThread_ = new boost::thread(attrs, boost::bind(&ServerEndpoint::CommEventMonitor, this));
#endif // GV_BACKUP

#ifdef GV_Scheduler
    sche = new Scheduler(clientID_);
#endif

#ifdef GV_eScheduler
    esche = new eScheduler(clientID_);
#endif

#ifdef GV_MSGHANDLER
    msghandler = new MsgHandler(clientID_);
#endif

    /* The client side should have initiated the connection, leading to this ep's creation */
    ucp_ep_params_t ep_params;
    ucs_status_t    status;
    ep_params.field_mask      = UCP_EP_PARAM_FIELD_ERR_HANDLER |
                                UCP_EP_PARAM_FIELD_CONN_REQUEST;
    ep_params.conn_request    = _connectReq;
    ep_params.err_handler.cb  = ErrorCallback;
    ep_params.err_handler.arg = &connStatus_;
    if ((status = ucp_ep_create(_dataWorker, &ep_params, &_clientEp)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_, "failed to create an endpoint (%s)\n", ucs_status_string(status));
        ucp_worker_destroy(_dataWorker);
        free(_connectReq); // todo
    }

    tool::Logging(LOG_COMM, myName_, "create the Server endpoint object (connected to client#%llu %s:%s).\n", clientID_, clientIP_, clientPort_);
}

void ServerEndpoint::NewRun() {
    tool::Logging(LOG_INFO, myName_, "start the server endpoint to serve the client#%llu.\n", clientID_);
    connStatus_.isClosed = false;

    RegisterHandler(__CUDA_REGISTER, __cudaRegisterHandle, _dataWorker, this);
    RegisterHandler(__CUDA_REGISTER_FAT_BINARY, __cudaRegisterFatBinaryHandle, _dataWorker, this);
    RegisterHandler(__CUDA_REGISTER_FUNCTION, __cudaRegisterFunctionHandle, _dataWorker, this);
    RegisterHandler(__CUDA_REGISTER_VAR, __cudaRegisterVarHandle, _dataWorker, this);
    RegisterHandler(CUDA_LAUNCH_KERNEL, cudaLaunchKernelHandle, _dataWorker, this);
    RegisterHandler(CUDA_FUNC_GET_ATTRIBUTES, cudaFuncGetAttributesHandle, _dataWorker, this);
    RegisterHandler(CUDA_MALLOC, cudaMallocHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMSET, cudaMemsetHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMSET_ASYNC, cudaMemsetAsyncHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEM_GET_INFO, cudaMemGetInfoHandle, _dataWorker, this);
    RegisterHandler(CUDA_FREE, cudaFreeHandle, _dataWorker, this);
    // RegisterHandler(CUDA_MEMCPY, cudaMemcpyHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMCPY_H2D, cudaMemcpyH2DHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMCPY_D2H, cudaMemcpyD2HHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMCPY_D2D, cudaMemcpyD2DHandle, _dataWorker, this);
    RegisterHandler(NEW_ITERATION_REQ, NewIterHandle, _dataWorker, this);
    // RegisterHandler(CUDA_MEMCPY_ASYNC, cudaMemcpyAsyncHandle, _dataWorker, this); //todo
    RegisterHandler(CUDA_MEMCPY_ASYNC_H2D, cudaMemcpyAsyncH2DHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMCPY_ASYNC_D2H, cudaMemcpyAsyncD2HHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMCPY_ASYNC_D2D, cudaMemcpyAsyncD2DHandle, _dataWorker, this);
    RegisterHandler(CUDA_MEMCPY_TO_SYMBOL, cudaMemcpyToSymbolHandle, _dataWorker, this);
    RegisterHandler(CUDA_DEVICE_SYNCHRONIZE, cudaDeviceSynchronizeHandle, _dataWorker, this);
    RegisterHandler(CUDA_GET_DEVICE, cudaGetDeviceHandle, _dataWorker, this);
    RegisterHandler(CUDA_GET_DEVICE_COUNT, cudaGetDeviceCountHandle, _dataWorker, this);
    RegisterHandler(CUDA_SET_MAIN_DEVICE, cudaSetMainDeviceHandle, _dataWorker, this);
    RegisterHandler(CUDA_SET_DEVICE, cudaSetDeviceHandle, _dataWorker, this);
    RegisterHandler(CUDA_GET_DEVICE_PROPERTIES, cudaGetDevicePropertiesHandle, _dataWorker, this);
    RegisterHandler(CUDA_DEVICE_GET_ATTRIBUTE, cudaDeviceGetAttributeHandle, _dataWorker, this);
    RegisterHandler(CUDA_EVENT_CREATE, cudaEventCreateHandle, _dataWorker, this);
    RegisterHandler(CUDA_EVENT_CREATE_WITH_FLAGS, cudaEventCreateWithFlagsHandle, _dataWorker, this);
    RegisterHandler(CUDA_EVENT_RECORD, cudaEventRecordHandle, _dataWorker, this);
    RegisterHandler(CUDA_EVENT_QUERY, cudaEventQueryHandle, _dataWorker, this);
    RegisterHandler(CUDA_EVENT_DESTROY, cudaEventDestroyHandle, _dataWorker, this);
    RegisterHandler(CUDA_EVENT_ELAPSED_TIME, cudaEventElapsedTimeHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_DESTROY, cudaStreamDestroyHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_CREATE, cudaStreamCreateHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_CREATE_WITH_FLAGS, cudaStreamCreateWithFlagsHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_CREATE_WITH_PRIORITY, cudaStreamCreateWithPriorityHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_WAIT_EVENT, cudaStreamWaitEventHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_SYNCHRONIZE, cudaStreamSynchronizeHandle, _dataWorker, this);  
    RegisterHandler(CUDA_STREAM_IS_CAPTURING, cudaStreamIsCapturingHandle, _dataWorker, this);
    RegisterHandler(CUDA_STREAM_GET_CAPTURE_INFO, cudaStreamGetCaptureInfoHandle, _dataWorker, this);
    RegisterHandler(CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR, cudaOccupancyMaxActiveBlocksPerMultiprocessorHandle, _dataWorker, this);
    RegisterHandler(CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR_WITH_FLAGS, cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlagsHandle, _dataWorker, this);


    RegisterHandler(CUBLAS_CREATE_V2, cublasCreate_v2Handle, _dataWorker, this);
    RegisterHandler(CUBLAS_SGEMM_V2, cublasSgemm_v2Handle, _dataWorker, this);
    RegisterHandler(CUBLAS_SGEMM_STRIDED_BATCHED, cublasSgemmStridedBatchedHandle, _dataWorker, this);
    RegisterHandler(CUBLAS_DESTROY_V2, cublasDestroy_v2Handle, _dataWorker, this);
    RegisterHandler(CUBLAS_SET_STREAM_V2, cublasSetStream_v2Handle, _dataWorker, this);
    RegisterHandler(CUBLAS_SET_WORKSPACE_V2, cublasSetWorkspace_v2Handle, _dataWorker, this);
    RegisterHandler(CUBLAS_SET_MATH_MODE, cublasSetMathModeHandle, _dataWorker, this);
    RegisterHandler(CUBLAS_GET_MATH_MODE, cublasGetMathModeHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_CREATE, cublasLtCreateHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_DESTROY, cublasLtDestroyHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULDESC_CREATE, cublasLtMatmulDescCreateHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULDESC_DESTROY, cublasLtMatmulDescDestroyHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULDESC_SETATTRIBUTE, cublasLtMatmulDescSetAttributeHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATRIX_LAYOUT_CREATE, cublasLtMatrixLayoutCreateHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATRIX_LAYOUT_DESTROY, cublasLtMatrixLayoutDestroyHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATRIX_LAYOUT_SETATTRIBUTE, cublasLtMatrixLayoutSetAttributeHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULPREFERENCE_CREATE, cublasLtMatmulPreferenceCreateHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULPREFERENCE_DESTROY, cublasLtMatmulPreferenceDestroyHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULPREFERENCE_SETATTRIBUTE, cublasLtMatmulPreferenceSetAttributeHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMULALGO_GETHEURISTIC, cublasLtMatmulAlgoGetHeuristicHandle, _dataWorker, this);
    RegisterHandler(CUBLASLT_MATMUL, cublasLtMatmulHandle, _dataWorker, this);


    RegisterHandler(CUDNN_CREATE, cudnnCreateHandle, _dataWorker, this);
    RegisterHandler(CUDNN_DESTROY, cudnnDestroyHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CREATE_TENSOR_DESCRIPTOR, cudnnCreateTensorDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_DESTROY_TENSOR_DESCRIPTOR, cudnnDestroyTensorDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_TENSOR_SIZE_IN_BYTES, cudnnGetTensorSizeInBytesHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_TENSOR_4D_DESCRIPTOR, cudnnSetTensor4dDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_TENSOR_ND_DESCRIPTOR, cudnnSetTensorNdDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_TENSOR_ND_DESCRIPTOR_EX, cudnnSetTensorNdDescriptorExHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CREATE_TENSOR_TRANSFORM_DESCRIPTOR, cudnnCreateTensorTransformDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_TENSOR_TRANSFORM_DESCRIPTOR, cudnnSetTensorTransformDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_DESTROY_TENSOR_TRANSFORM_DESCRIPTOR, cudnnDestroyTensorTransformDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_INIT_TRANSFORM_DEST, cudnnInitTransformDestHandle, _dataWorker, this);
    RegisterHandler(CUDNN_TRANSFORM_TENSOR_EX, cudnnTransformTensorExHandle, _dataWorker, this);
    RegisterHandler(CUDNN_TRANSFORM_FILTER, cudnnTransformFilterHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CREATE_FILTER_DESCRIPTOR, cudnnCreateFilterDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_FILTER_ND_DESCRIPTOR, cudnnSetFilterNdDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_DESTROY_FILTER_DESCRIPTOR, cudnnDestroyFilterDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_FILTER_SIZE_IN_BYTES, cudnnGetFilterSizeInBytesHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_FOLDED_CONV_BACKWARD_DATA_DESCRIPTORS, cudnnGetFoldedConvBackwardDataDescriptorsHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_STREAM, cudnnSetStreamHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BATCH_NORMALIZATION_BACKWARD_EX, cudnnBatchNormalizationBackwardExHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BATCH_NORMALIZATION_FORWARD_TRAINING_EX, cudnnBatchNormalizationForwardTrainingExHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BATCH_NORMALIZATION_FORWARD_INFERENCE, cudnnBatchNormalizationForwardInferenceHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BACKEND_CREATE_DESCRIPTOR, cudnnBackendCreateDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BACKEND_DESTROY_DESCRIPTOR, cudnnBackendDestroyDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BACKEND_SET_ATTRIBUTE, cudnnBackendSetAttributeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BACKEND_GET_ATTRIBUTE, cudnnBackendGetAttributeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BACKEND_EXECUTE, cudnnBackendExecuteHandle, _dataWorker, this);
    RegisterHandler(CUDNN_BACKEND_FINALIZE, cudnnBackendFinalizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_BATCH_NORMALIZATION_BACKWARD_EX_WORKSPACE_SIZE, cudnnGetBatchNormalizationBackwardExWorkspaceSizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_BATCH_NORMALIZATION_FORWARD_TRAINING_EX_WORKSPACE_SIZE, cudnnGetBatchNormalizationForwardTrainingExWorkspaceSizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_BATCH_NORMALIZATION_TRAINING_EX_RESERVE_SPACE_SIZE, cudnnGetBatchNormalizationTrainingExReserveSpaceSizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CREATE_CONVOLUTION_DESCRIPTOR, cudnnCreateConvolutionDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_DESTROY_CONVOLUTION_DESCRIPTOR, cudnnDestroyConvolutionDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_CONVOLUTION_GROUP_COUNT, cudnnSetConvolutionGroupCountHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_CONVOLUTION_MATH_TYPE, cudnnSetConvolutionMathTypeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_CONVOLUTION_ND_DESCRIPTOR, cudnnSetConvolutionNdDescriptorHandle, _dataWorker, this);
    RegisterHandler(CUDNN_SET_CONVOLUTION_REORDER_TYPE, cudnnSetConvolutionReorderTypeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_CONVOLUTION_FORWARD_ALGORITHM_V7, cudnnGetConvolutionForwardAlgorithm_v7Handle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_ALGORITHM_V7, cudnnGetConvolutionBackwardFilterAlgorithm_v7Handle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_CONVOLUTION_BACKWARD_DATA_ALGORITHM_V7, cudnnGetConvolutionBackwardDataAlgorithm_v7Handle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_CONVOLUTION_FORWARD_WORKSPACE_SIZE, cudnnGetConvolutionForwardWorkspaceSizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CONVOLUTION_FORWARD, cudnnConvolutionForwardHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_CONVOLUTION_BACKWARD_DATA_WORKSPACE_SIZE, cudnnGetConvolutionBackwardDataWorkspaceSizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CONVOLUTION_BACKWARD_FILTER, cudnnConvolutionBackwardFilterHandle, _dataWorker, this);
    RegisterHandler(CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_WORKSPACE_SIZE, cudnnGetConvolutionBackwardFilterWorkspaceSizeHandle, _dataWorker, this);
    RegisterHandler(CUDNN_CONVOLUTION_BACKWARD_DATA, cudnnConvolutionBackwardDataHandle, _dataWorker, this);
    RegisterHandler(NCCL_GROUP_START, ncclGroupStartHandle, _dataWorker, this);
    RegisterHandler(NCCL_GROUP_END, ncclGroupEndHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_INIT_RANK, ncclCommInitRankHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_DESTROY, ncclCommDestroyHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_GET_ASYNC_ERROR, ncclCommGetAsyncErrorHandle, _dataWorker, this);
    RegisterHandler(NCCL_GET_UNIQUE_ID, ncclGetUniqueIdHandle, _dataWorker, this);
    RegisterHandler(NCCL_GET_VERSION, ncclGetVersionHandle, _dataWorker, this);
    RegisterHandler(NCCL_ALL_REDUCE, ncclAllReduceHandle, _dataWorker, this);
    RegisterHandler(NCCL_REDUCE, ncclReduceHandle, _dataWorker, this);
    RegisterHandler(NCCL_REDUCE_SCATTER, ncclReduceScatterHandle, _dataWorker, this);
    RegisterHandler(NCCL_ALL_GATHER, ncclAllGatherHandle, _dataWorker, this);
    RegisterHandler(NCCL_BROADCAST, ncclBroadcastHandle, _dataWorker, this);
    RegisterHandler(NCCL_SEND, ncclSendHandle, _dataWorker, this);
    RegisterHandler(NCCL_RECV, ncclRecvHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_COUNT, ncclCommCountHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_USER_RANK, ncclCommUserRankHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_CU_DEVICE, ncclCommCuDeviceHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_ABORT, ncclCommAbortHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_INIT_ALL, ncclCommInitAllHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_INIT_RANK_CONFIG, ncclCommInitRankConfigHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_SPLIT, ncclCommSplitHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_FINALIZE, ncclCommFinalizeHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_REGISTER, ncclCommRegisterHandle, _dataWorker, this);
    RegisterHandler(NCCL_COMM_DEREGISTER, ncclCommDeregisterHandle, _dataWorker, this);
    RegisterHandler(NCCL_MEM_ALLOC, ncclMemAllocHandle, _dataWorker, this);
    RegisterHandler(NCCL_MEM_FREE, ncclMemFreeHandle, _dataWorker, this);
    RegisterHandler(NCCL_RED_OP_CREATE_PRE_MUL_SUM, ncclRedOpCreatePreMulSumHandle, _dataWorker, this);
    RegisterHandler(NCCL_RED_OP_DESTROY, ncclRedOpDestroyHandle, _dataWorker, this);

    while (!connStatus_.isClosed) {
        uint res = ucp_worker_progress(_dataWorker);
#ifdef GV_Scheduler
        if(res != 0 && CliDev_ != -1){
            sche->enqueue(clientID_, CliDev_);
#ifdef GV_Monitor
            if(getDeviceFlag == 0){
                std::string message = "client_runjob:";
                sche->send_message(message,CliDev_);
                getDeviceFlag = 1;
            }
#endif
        }
#endif

#ifdef GV_MSGHANDLER
        if(res != 0 && CliDev_ != -1 && replayFlag_ && !createThreadFlag){
            msghandler->replay_flag = true;
            createThreadFlag = true;
            boost::thread_attributes attrs;
            attrs.set_stack_size(THREAD_STACK_SIZE);
            MSGRecvThread_ = new boost::thread(attrs, boost::bind(&MsgHandler::aysnc_receive_message, msghandler));
            MSGRecvThread_->detach();
        }
        MSGStopFlag_ = msghandler->stop_flag;

#endif

#ifdef GV_eScheduler
        if(res != 0 && CliDev_ != -1){
            esche->enqueue(clientID_, CliDev_, priority_);
        }
#endif

    }

    // this->~ServerEndpoint();
    delete this;
}

DEFINE_SERVER_AM_CALLBACK(NewIterHandle) {
    const char* myName = "NewIteration";
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    size_t          tensorNum   = reqBuf.Pop<size_t>();
    TensorInfo_t*   tensors     = reqBuf.AssignAddrForAll<TensorInfo_t>();
    serverEp->trainTensors_.clear();
    size_t totalTensorSize = 0;
    for (size_t i = 0; i < tensorNum; i++) {
        serverEp->trainTensors_.push_back(tensors[i]);
        totalTensorSize += tensors[i].size;
    }

    serverEp->curIter_ ++;
    tool::Logging(LOG_DEBUG, myName, "client#%llu starts the iteration#%zu.\n", serverEp->clientID_, serverEp->curIter_);
    if (serverEp->curIter_ > 1 && serverEp->_cuInfoMap.isFirstIter) {
        serverEp->_cuInfoMap.isFirstIter = false;
    }

#ifdef GV_eScheduler
    serverEp->esche->cal_add_It(serverEp->clientID_);
#endif

#ifdef GV_BACKUP
    if (serverEp->curIter_ > 0 && serverEp->curIter_ % BACKUP_PERIOD == 0 
    && !serverEp->recoveryFlag_) { // avoid backup in replay mode
        auto start_back = std::chrono::high_resolution_clock::now();
        serverEp->Backup2Memory();
        auto end_back = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration_back = end_back - start_back;
        tool::Logging(myName, "backup total time=%f s\n", duration_back.count());
        serverEp->ckptCnt++; // debug
    }

    if (serverEp->curIter_ >= BACKUP_PERIOD && !serverEp->recoveryFlag_) {
        // serverEp->Backup2Storage(serverEp->curIter_);
        auto start = std::chrono::high_resolution_clock::now();
        if (!serverEp->trainTensorBackup_.CheckStart()) {
            serverEp->trainTensorBackup_.Start(boost::bind(&ServerEndpoint::BackupTrainTensors2Storage, serverEp));
        }
        serverEp->trainTensorBackup_.Notify();
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        tool::Logging(LOG_DEBUG, myName, "submit the backup task for iteration#%zu in %f seconds.\n", serverEp->curIter_, duration.count());
    }
    else if (serverEp->recoveryFlag_) {
        serverEp->LoadFromStorage(serverEp->curIter_ - 1); // curIter_ starts from 1 while restoring
    }


/*
    if (serverEp->curIter_ == BACKUP_PERIOD && !serverEp->recoveryFlag_) {
        cudaError_t exit_code = cudaMallocHost(&serverEp->backupMemcpyBuffer_, totalTensorSize * BACKUP_PERIOD);
        if (exit_code != cudaSuccess) {
            tool::Logging(LOG_ERROR, myName, "cudaMallocHost failed for backup memcpy buffer: %s\n", cudaGetErrorString(exit_code));
            exit(EXIT_FAILURE);
        }
    }
    if (serverEp->backupMemcpyBuffer_ && !serverEp->recoveryFlag_) {
        auto start = std::chrono::high_resolution_clock::now();
        size_t offset = (serverEp->curIter_ % BACKUP_PERIOD) * totalTensorSize; // buffer starts from curIter_ = 0
        for (size_t i = 0; i < tensorNum; i++) {
            int streamIdx = i % BACKUP_STREAM_NUM;
            // cudaError_t exit_code = cudaMemcpy((uint8_t*)serverEp->backupMemcpyBuffer_ + offset, serverEp->GetDevPtr((uint64_t)tensors[i].devPtr), tensors[i].size, cudaMemcpyDeviceToHost);
            cudaError_t exit_code = cudaMemcpyAsync((uint8_t*)serverEp->backupMemcpyBuffer_ + offset, serverEp->GetDevPtr((uint64_t)tensors[i].devPtr), tensors[i].size, cudaMemcpyDeviceToHost, serverEp->streamList_[streamIdx]);
            if (exit_code != cudaSuccess) {
                tool::Logging(LOG_ERROR, myName, "cudaMemcpy failed for tensor[%zu](%p) with size %zu: %s\n", i, tensors[i].devPtr, tensors[i].size, cudaGetErrorString(exit_code));
                exit(EXIT_FAILURE);
            }
            else {
                tool::Logging(LOG_DEBUG, myName, "cudaMemcpy for tensor[%zu](%p) with size %zu in stream %p.\n", i, tensors[i].devPtr, tensors[i].size, serverEp->streamList_[streamIdx]);
            }

            offset += tensors[i].size;
        }
        auto end = std::chrono::high_resolution_clock::now();
        tool::Logging(LOG_DEBUG, myName, "cudaMemcpy for %zu bytes takes %f seconds\n", totalTensorSize, std::chrono::duration<double>(end - start).count());
    }
    if (serverEp->recoveryFlag_) {
        size_t offset = ((serverEp->curIter_ - 1) % BACKUP_PERIOD) * totalTensorSize; // curIter_ starts from 1
        for (size_t i = 0; i < tensorNum; i++) {
            cudaError_t exit_code = cudaMemcpyAsync(serverEp->GetDevPtr((uint64_t)tensors[i].devPtr), (uint8_t*)serverEp->backupMemcpyBuffer_ + offset, tensors[i].size, cudaMemcpyHostToDevice, serverEp->defaultStream_);
            // cudaMemcpy(serverEp->GetDevPtr((uint64_t)tensors[i].devPtr), (uint8_t*)serverEp->backupMemcpyBuffer_ + offset, tensors[i].size, cudaMemcpyHostToDevice);
            offset += tensors[i].size;
        } 
        // cudaStreamSynchronize(serverEp->defaultStream_);
        tool::Logging(LOG_DEBUG, myName, "recovery the tensors for iteration#%zu in stream %p.\n", serverEp->curIter_, serverEp->defaultStream_);
    }
*/

#endif // GV_BACKUP

    return UCS_OK;
}

void ServerEndpoint::CreateServerEp(ucp_conn_request_h conn_request, void *arg) {
    const char* myName = "CreateServerEp";
    UCPConnection* connectionObj = (UCPConnection*)arg;
    UCPConnection_t conn;

    char* ip_str = (char*)malloc(IP_STRING_LEN);
    char* port_str = (char*)malloc(PORT_STRING_LEN);
    ucp_conn_request_attr_t attr;
    attr.field_mask = UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ADDR | UCP_CONN_REQUEST_ATTR_FIELD_CLIENT_ID;
    ucs_status_t status = ucp_conn_request_query(conn_request, &attr); 
    if (status == UCS_OK) { // get the client address from the connection request
        tool::GetIpStrFromSockaddr(&attr.client_address, ip_str, IP_STRING_LEN);
        tool::GetPortStrFromSockaddr(&attr.client_address, port_str, PORT_STRING_LEN);
        tool::Logging(LOG_INFO, myName, "recv a connection request(%s:%s) from client#%llu.\n", ip_str, port_str, attr.client_id); 
        conn.client_id = attr.client_id;
        conn.client_ip = ip_str;
        conn.client_port = port_str;
        conn.conn_request = conn_request;

    } else if (status != UCS_ERR_UNSUPPORTED) {
        tool::Logging(LOG_ERROR, myName, "failed to query the connection request (%s)\n", ucs_status_string(status)); // todo: UCS_ERR_UNSUPPORTED
        exit(EXIT_FAILURE);
    }

    ServerEndpoint* serverEndPointObj = new ServerEndpoint(connectionObj->CreateWorker(), conn);

    boost::thread_attributes attrs;
    attrs.set_stack_size(THREAD_STACK_SIZE);
    boost::thread* thTmp = new boost::thread(attrs, boost::bind(&ServerEndpoint::NewRun, serverEndPointObj));
    thTmp->detach();
    delete thTmp;
}


/**
 * Close UCP endpoint.
 */
void ServerEndpoint::CloseEp(uint64_t flags) {
    ucp_request_param_t param;
    ucs_status_t status;
    void *close_req;

    param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    param.flags        = flags;
    close_req          = ucp_ep_close_nbx(_clientEp, &param);
    if (UCS_PTR_IS_PTR(close_req)) {
        do {
            ucp_worker_progress(_dataWorker);
            status = ucp_request_check_status(close_req);
        } while (status == UCS_INPROGRESS);
        ucp_request_free(close_req);
    } else {
        status = UCS_PTR_STATUS(close_req);
    }

    if (status != UCS_OK) {
        fprintf(stderr, "failed to close ep %p: %s\n", (void*)_clientEp,
                ucs_status_string(status));
    }
}

ServerEndpoint::~ServerEndpoint() {
    CloseEp(UCP_EP_CLOSE_MODE_FLUSH);
    ucp_worker_destroy(_dataWorker);
    //free(_connectReq);
    //ucp_listener_reject(_serverConnection.listener, _serverConnection.conn_request);
    // if (UCS_PTR_IS_PTR(_connectReq)) {
    //     ucp_request_free(_connectReq);
    // } //! In multi-threaded version, it may cause the error

#ifdef GV_Scheduler
    if(CliDev_ != -1){
        sche->free_jobs(clientID_, CliDev_);
#ifdef GV_Monitor
        std::string message = "client_stop:";
        sche->send_message(message,CliDev_);
#endif
    }
#endif

#ifdef GV_eScheduler
    if(CliDev_ != -1){
        esche->free_jobs(clientID_, CliDev_);
    }
    delete esche;
#endif


    for (int i = 0; i < 3; i++) {
        delete shmQueSizes_[i];
        delete shmQueues_[i];
    }

#ifdef GV_MSGHANDLER
    // MSGRecvThread_->join();
    delete msghandler;
    // MSGRecvThread_->interrupt();
    // delete MSGRecvThread_;
#endif


#ifdef GV_Scheduler
#ifdef GV_Monitor
    bool isClosed = sche->ready_to_del();
    if(isClosed){
        delete sche;
    }
#ifndef GV_Monitor
    delete sche;
#endif
#endif

#endif
    // delete[] blockList_;
    

    __cudaUnregisterFatBinaryHandle();
    
    tool::Logging(LOG_INFO, myName_, "close the Server endpoint object.\n");
    // printf("zwx mapFatBinary=%p, mapFatBinHandle2CuModule=%p\n", _cuInfoMap.mapFatBinary, _cuInfoMap.mapFatBinHandle2CuModule);
    free(myName_);
    free(clientIP_);
    free(clientPort_);
}

void ServerEndpoint::SendResponse(RequestIOV* reqBuffer, const ucp_ep_h* ep, ucs_memory_type_t memType) {
    SendData(reqBuffer->GetIOVs(), reqBuffer->GetNum(),
         reqBuffer->GetHeaders(), reqBuffer->GetHeaderSize(), 
         (reqBuffer->GetThreadID() << RECV_AM_SHIFT_BIT) + reqBuffer->GetRequestType(), 
         &_dataWorker, (ucp_ep_h*)ep, false, memType);
}

void ServerEndpoint::SendStatus(int status) {
    RequestIOV resBuf;
    resBuf.PushRequestType(SERVER_STATUS);
    resBuf.Push(status);
    int dummy = 0;
    resBuf.PushThreadID(dummy);
    SendData(resBuf.GetIOVs(), resBuf.GetNum(),
                resBuf.GetHeaders(), resBuf.GetHeaderSize(), 
                resBuf.GetRequestType(), &_dataWorker, &_clientEp);
}

void ServerEndpoint::UpdateStream(cudaStream_t stream, bool isCommStream) {
    if (isCommStream) {
        commStream_ = stream;
    } else {
        defaultStream_ = stream;
    }
}
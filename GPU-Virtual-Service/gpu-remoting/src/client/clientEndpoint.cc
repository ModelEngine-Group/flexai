#include "../../include/clientEndpoint.h"

ClientEndpoint::ClientEndpoint(uint64_t clientID, size_t priority, ucp_worker_h clientWorker, int dev){
    _clientID = clientID;
    priority_ = priority;
    _dataWorker = clientWorker;

    clientIP_ = (char*)malloc(IP_STRING_LEN);
    clientPort_ = (char*)malloc(PORT_STRING_LEN);

    _myDevIdx = dev;

    _threadID = static_cast<pid_t>(::syscall(SYS_gettid));
    _processID = getpid();
    Connect();

    _shmOpt = shmOpt;

    tool::Logging(LOG_DEBUG, myName_, "[pid:%d, tid:%d] client endpoint object is created successfully.\n", _processID, _threadID);
}

void ClientEndpoint::Connect(bool replay) {
    uint16_t serverPort;
#ifdef GV_GPUMAP
    GpuInfoEntry_t* ginfo;

#ifdef GV_BACKUP
    if(replay){
        std::cout << "replay" << std::endl;
        gpuIdMap->ReallocGPU();
    }
#endif

    gpuIdMap->GetGPUinfo(_myDevIdx, &ginfo);
    const std::string serverIP(ginfo->nodeIp); 
    serverPort = ginfo->nodePort;
    shmOpt->WriteIpPort(ginfo->dataPort, ginfo->dataIp);
#else
    const std::string& serverIP = config_->GetServerIp();
    serverPort = config_->GetServerPort();
#endif

    ucp_ep_params_t ep_params;
    ucs_status_t    status;
    struct sockaddr_storage serverAddr;
    tool::SetSockAddr(serverIP.c_str(), serverPort, &serverAddr, AF_INET);
    ep_params.field_mask                = UCP_EP_PARAM_FIELD_FLAGS       |
                                          UCP_EP_PARAM_FIELD_SOCK_ADDR   |
                                          UCP_EP_PARAM_FIELD_ERR_HANDLER |
                                          UCP_EP_PARAM_FIELD_ERR_HANDLING_MODE;                                          

    ep_params.err_mode                  = UCP_ERR_HANDLING_MODE_PEER;
    ep_params.err_handler.cb            = ClientErrorCallback;
    ep_params.err_handler.arg           = &connStatus_;
    ep_params.flags                     = UCP_EP_PARAMS_FLAGS_CLIENT_SERVER |
                                          UCP_EP_PARAMS_FLAGS_SEND_CLIENT_ID;
    ep_params.sockaddr.addr             = (const struct sockaddr*)&serverAddr;
    ep_params.sockaddr.addrlen          = sizeof(struct sockaddr_storage);

    if ((status = ucp_ep_create(_dataWorker, &ep_params, &_serverEp)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_, "failed to create an endpoint and connect to the server (%s)\n", ucs_status_string(status));
        ucp_worker_destroy(_dataWorker);
    }
    else {
        ucp_am_handler_param_t param;
        param.field_mask = UCP_AM_HANDLER_PARAM_FIELD_ID |
                           UCP_AM_HANDLER_PARAM_FIELD_CB |
                           UCP_AM_HANDLER_PARAM_FIELD_ARG;
        param.id         = SERVER_STATUS;
        param.cb         = ServerStatusCallback;
        param.arg        = this;
        if ((status = ucp_worker_set_am_recv_handler(_dataWorker, &param)) != UCS_OK) {
            tool::Logging(LOG_ERROR, myName_, "failed to set am handler (%s)\n", ucs_status_string(status));
        }
        else {
            SendMainDevice(replay);
        }
    }
}

/**
 * Close UCP endpoint.
 */
void ClientEndpoint::CloseEp(uint64_t flags) {
    ucp_request_param_t param;
    ucs_status_t status;
    void *close_req;

    param.op_attr_mask = UCP_OP_ATTR_FIELD_FLAGS;
    param.flags        = flags;
    close_req          = ucp_ep_close_nbx(_serverEp, &param);
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
        fprintf(stderr, "failed to close ep %p: %s\n", (void*)_serverEp,
                ucs_status_string(status));
    }
}

ClientEndpoint::~ClientEndpoint() {
    CloseEp(UCP_EP_CLOSE_MODE_FLUSH);
    ucp_worker_destroy(_dataWorker);

    // if (_shmOpt != NULL) { //todo
    //     delete _shmOpt;
    // } 
    tool::Logging(LOG_INFO, myName_, "close the client endpoint object(GPU #%d).\n", _myDevIdx);
}

ucs_status_t ClientEndpoint::SendRequest(RequestIOV* reqBuffer, bool forcedEager, bool isCheckpoint) {
    tool::Logging(LOG_COMM, "SendRequest", "ready prepare amID: %d, apiID: %d, ttID: %d\n", (ttID << RECV_AM_SHIFT_BIT) + reqBuffer->GetRequestType(), reqBuffer->GetRequestType(), ttID);

    reqBuffer->PushThreadID(ttID);
    ucs_status_t status = SendData(reqBuffer->GetIOVs(), reqBuffer->GetNum(),
         reqBuffer->GetHeaders(), reqBuffer->GetHeaderSize(), 
         reqBuffer->GetRequestType(), &_dataWorker, &_serverEp,
          true, UCS_MEMORY_TYPE_HOST, forcedEager);
#ifdef GV_BACKUP
    if (status == UCS_ERR_CONNECTION_RESET) {
        {
            std::unique_lock<std::mutex> lock(reConnectMutex);
            if (!isReConnected) { // only the first thread can reconnect
                tool::Logging(LOG_INFO, "SendRequest", "ready to reconnect...\n");
                sleep(10);
                Connect(true);
                isReConnected = true;
            }
        }

        tool::Logging(LOG_INFO, "SendRequest", "[pid:%d, ttid:%d] ready to resend previous request (requestType: %d)\n", _processID, ttID, reqBuffer->GetRequestType());
        ucs_status_t status = SendData(reqBuffer->GetIOVs(), reqBuffer->GetNum(),
         reqBuffer->GetHeaders(), reqBuffer->GetHeaderSize(), 
         reqBuffer->GetRequestType(), &_dataWorker, &_serverEp,
          true, UCS_MEMORY_TYPE_HOST, forcedEager);
        if (status != UCS_OK) {
            tool::Logging(LOG_ERROR, "SendRequest", "[pid:%d, ttid:%d] failed to resend the request (requestType: %d)\n", _processID, ttID, reqBuffer->GetRequestType());
            exit(EXIT_FAILURE);
        }
    }
    // isCheckpoint = false;
    if (isCheckpoint) {
        UpdateReqIOVList(reqBuffer);
    }
#endif // GV_BACKUP
    CheckTensors(reqBuffer->GetRequestType());
    return status;
}

void ClientEndpoint::SendRegisterRequest(ClientEndpoint* curEp, bool forcedEager) {
    // tool::Logging(LOG_COMM, "SendRequest", "ready prepare amID: %d, apiID: %d, ttID: %d\n", (ttID << RECV_AM_SHIFT_BIT) + reqBuffer->GetRequestType(), reqBuffer->GetRequestType(), ttID);

    // reqBuffer->PushThreadID(ttID);
    // reqBuffer->Print();
    for (RegisterIOV* reqBuffer : regIOVList) {
        SendData(reqBuffer->GetIOVs(), reqBuffer->GetNum(),
                reqBuffer->GetUcpHeaders(), reqBuffer->GetUcpHeaderSize(), 
                reqBuffer->GetRequestType(), &curEp->_dataWorker, &curEp->_serverEp,
                true, UCS_MEMORY_TYPE_HOST, forcedEager);
    }
}

void ClientEndpoint::SendRequestH2D(RequestIOV* reqBuffer, uint8_t* header, size_t headerSize, bool forcedEager, bool isCheckpoint) {
    // no response required for RNDV request, so no need to push threadID
    SendData(reqBuffer->GetIOVs(), reqBuffer->GetNum(),
         (size_t*)header, headerSize, 
         reqBuffer->GetRequestType(), &_dataWorker, &_serverEp,
          false, UCS_MEMORY_TYPE_HOST, forcedEager);
    CheckTensors(reqBuffer->GetRequestType());

#ifdef GV_BACKUP
    if (memcpyRecord_ && isCheckpoint) {
        UpdateReqIOVList(reqBuffer, header, headerSize);
    }
#endif // GV_BACKUP
}

void ClientEndpoint::SendNewIterRequest(size_t iterNum) {
    // Send a request to the server to notify the start of a new iteration
    RequestIOV reqBuffer;
    reqBuffer.PushRequestType(NEW_ITERATION_REQ);
    reqBuffer.PushConst(tensorByteList.size());
    reqBuffer.Push(tensorByteList.data(), tensorByteList.size());
    SendRequest(&reqBuffer, true);

#ifdef GV_BACKUP
    if (iterNum + 1 == BACKUP_PERIOD) {
        memcpyRecord_ = true; // prepare for recording the memcpy requests for the next iteration (before shrinking the list)
    }
    if (iterNum > 0 && iterNum % BACKUP_PERIOD == 0) {
        Checkpointing();
    }
#endif // BACKUP

    // cudaStreamSynchronize(NULL);
}

void ClientEndpoint::SendMainDevice(bool replay) {
    bool loadData = replay;
    int gpuIdInNode = _myDevIdx;
#ifdef GV_GPUMAP
    gpuIdMap->GetGPUId(_myDevIdx, &gpuIdInNode);
#endif // GV_GPUMAP

    RequestIOV reqBuffer;
    reqBuffer.PushRequestType(CUDA_SET_MAIN_DEVICE);
    reqBuffer.Push(gpuIdInNode);
    reqBuffer.Push(_processID);
    reqBuffer.Push(priority_);
    reqBuffer.Push(loadData);
    reqBuffer.PushThreadID(ttID);
    SendData(reqBuffer.GetIOVs(), reqBuffer.GetNum(),
         reqBuffer.GetHeaders(), reqBuffer.GetHeaderSize(), 
         reqBuffer.GetRequestType(), &_dataWorker, &_serverEp,
          false, UCS_MEMORY_TYPE_HOST, true);
    

    ucp_ep_attr_t ep_attr;
    ep_attr.field_mask = UCP_EP_ATTR_FIELD_LOCAL_SOCKADDR |
                            UCP_EP_ATTR_FIELD_REMOTE_SOCKADDR;
    ucs_status_t status = ucp_ep_query(_serverEp, &ep_attr);
    if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_, "failed to query the endpoint: %s\n", ucs_status_string(status));
        return;
    }
    else {
        tool::GetIpStrFromSockaddr(&ep_attr.local_sockaddr, clientIP_, IP_STRING_LEN);
        tool::GetPortStrFromSockaddr(&ep_attr.local_sockaddr, clientPort_, PORT_STRING_LEN);
        tool::Logging(LOG_INFO, myName_, "[pid:%d, tid:%d] client(%s:%s) has connected to server and will use GPU #%d(IDinNode:#%d)\n", _processID, _threadID, clientIP_, clientPort_, _myDevIdx, gpuIdInNode);
    }
    connStatus_.isClosed = false;

    if (replay) {
        Replay();
    }
}

ucs_status_t ClientEndpoint::SendRequestRecvResponse(RequestIOV* reqBuffer, RequestIOV* responseBuffer, bool forcedEager, bool isCheckpoint) {
    const char* myName = "SendRequestRecvResponse";
    ucs_status_t status = UCS_OK;
    // isCheckpoint = false;

    // prepare for receiving response first (avoid receiving message before setting the handler)
    ucp_dt_iov_t *iov   = responseBuffer->GetIOVs();
    size_t       iovNum = responseBuffer->GetNum();
    NewActiveMessageDesc_t am_request_ctx = { .complete = 0, .is_rndv = 0, .mem_type = UCS_MEMORY_TYPE_HOST, .desc = NULL, 
                                           .iov = iov, .iov_num = iovNum};
                                        //    .iov = iov, .iov_num = iovNum, .send_amID = ((ttID << RECV_AM_SHIFT_BIT) + reqBuffer->GetRequestType())};
    Request_t recv_request_ctx = {.type = 1, .complete = 0};
    ucp_am_handler_param_t param1;
    param1.field_mask   = UCP_AM_HANDLER_PARAM_FIELD_ID |
                          UCP_AM_HANDLER_PARAM_FIELD_CB |
                          UCP_AM_HANDLER_PARAM_FIELD_ARG;
    param1.id           = (ttID << RECV_AM_SHIFT_BIT) + reqBuffer->GetRequestType(); 
    param1.cb           = RetrieveData;
    param1.arg          = &am_request_ctx;
    tool::Logging(LOG_COMM, myName, "amID: %d, requestType: %d, ttID: %d\n", param1.id, reqBuffer->GetRequestType(), ttID);
    status = ucp_worker_set_am_recv_handler(_dataWorker, &param1);
    if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName, "failed to set am handler: %s\n", ucs_status_string(status));
        return status;
    }
    else {
        tool::Logging(LOG_COMM, myName, "set am handler successfully for param1.id: %d\n", param1.id);
    }

    // send request and wait for completion
    SendRequest(reqBuffer, forcedEager, false);

    // wait for response
    tool::Logging(LOG_COMM, myName, "waiting for server to send response.\n");
    while (!am_request_ctx.complete && !connStatus_.isClosed) { // waiting ActiveMessageRecvCallback() to be invoked
        ucp_worker_progress(_dataWorker);
    }
    if (connStatus_.isClosed) {
        tool::Logging(LOG_DEBUG, myName, "connection is closed, try to resend the request.\n");
        
        // re-send request and wait for completion
        SendRequest(reqBuffer, forcedEager, false);
        ucp_worker_set_am_recv_handler(_dataWorker, &param1); // make sure the handler is set
        tool::Logging(LOG_COMM, myName, "waiting for server to send response again.\n");
        while (!am_request_ctx.complete && !connStatus_.isClosed) { // waiting ActiveMessageRecvCallback() to be invoked
            ucp_worker_progress(_dataWorker);
        }
        if (connStatus_.isClosed) {
            tool::Logging(LOG_ERROR, myName, "connection is still closed, failed to receive response.\n");
            return UCS_ERR_CONNECTION_RESET;
        }
        tool::Logging(LOG_COMM, myName, "response has arrived after re-sending the request.\n");
    }

    if (!am_request_ctx.is_rndv) {
        tool::Logging(LOG_COMM, myName, "Eager response has arrived.\n");
    }
    else {
        tool::Logging(LOG_COMM, myName, "Rendezvous response has arrived.\n");
        recv_request_ctx.complete = 0;
        ucp_request_param_t param2;
        param2.op_attr_mask     = UCP_OP_ATTR_FIELD_CALLBACK |
                                  UCP_OP_ATTR_FIELD_DATATYPE |
                                  UCP_OP_ATTR_FIELD_USER_DATA|
                                  UCP_OP_ATTR_FIELD_MEMORY_TYPE;
        param2.op_attr_mask    |= UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
        param2.datatype         = ucp_dt_make_contig(1);
        param2.user_data        = &recv_request_ctx;
        param2.cb.recv_am       = (ucp_am_recv_data_nbx_callback_t)RecvCallBack;
        param2.memory_type      = UCS_MEMORY_TYPE_HOST;
        Request_t* rndv_request = (Request_t*)ucp_am_recv_data_nbx(_dataWorker,
                                              am_request_ctx.desc,
                                              iov[0].buffer, iov[0].length, 
                                             &param2);
        status = Wait(rndv_request, &recv_request_ctx, &_dataWorker); 
        if (status != UCS_OK) {
            tool::Logging(LOG_ERROR, myName, "ucp_am_recv_data_nbx failed: %s\n", ucs_status_string(status));
        }
        else {
            tool::Logging(LOG_COMM, myName, "ucp_am_recv_data_nbx completed successfully.\n");
        }
        ucp_request_free(rndv_request);
    }

#ifdef GV_BACKUP
    if (isCheckpoint) {
        int reqType = reqBuffer->GetRequestType();
        if (std::binary_search(std::begin(CreateAPIs), std::end(CreateAPIs), reqType)) {
            UpdateReqIOVList(reqBuffer);
        }
        else if (reqType == CUDA_STREAM_SYNCHRONIZE) {
            UpdateReqIOVList(reqBuffer);
        }
        else if (reqType == CUDA_MALLOC) {
            UpdateReqIOVList(reqBuffer);
        }
    }
#endif // GV_BACKUP

    return status;
}
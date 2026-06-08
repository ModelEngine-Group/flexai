#ifndef UCP_UTIL_H
#define UCP_UTIL_H

#include "configure.h"
#include <ucp/api/ucp.h>

#ifdef USE_CUDA
#include <cuda_runtime.h>
#endif

/* ---- define the structure ---- */

typedef struct {
    // int                         idx             = -1;
    volatile ucp_conn_request_h conn_request    = NULL;
    // ucp_listener_h              listener        = NULL;
    uint64_t                    client_id       = -1;
    char                        *client_ip      = NULL;
    char                        *client_port    = NULL;
} UCPConnection_t; // used in the user's connection request callback.

typedef struct {
    volatile int                complete;
    int                         is_rndv;
    ucs_memory_type_t           mem_type;
    void                        *desc;
    ucp_dt_iov_t                *iov;
    size_t                      iov_num;
    void                        *recv_buf;
    size_t                      recv_length;
} ActiveMessageDesc_t; // descriptor of the data received with AM API

typedef struct {
    volatile int                complete;
    int                         is_rndv;
    ucs_memory_type_t           mem_type;
    void                        *desc;
    ucp_dt_iov_t                *iov;
    size_t                      iov_num;
    // int                         send_amID;
} NewActiveMessageDesc_t;

typedef struct {
    int type;     // send or recv
    int complete; // indicate whether or not the request is completed
} Request_t; // request context

typedef enum {
    CLIENT_SERVER_SEND_RECV_STREAM  = UCS_BIT(0),
    CLIENT_SERVER_SEND_RECV_TAG     = UCS_BIT(1),
    CLIENT_SERVER_SEND_RECV_AM      = UCS_BIT(2),
    CLIENT_SERVER_SEND_RECV_DEFAULT = CLIENT_SERVER_SEND_RECV_AM
} SendRecvType;

typedef struct {
    bool isServer; // indicate the current node is server or client
    bool isClosed; // indicate whether or not the connection is closed
} ConnStatus_t;

/* ---- define the call back function ---- */

#define DECLARE_AM_CALLBACK(name) \
    static ucs_status_t name(void *arg, const void *header, size_t header_length, \
                             void *data, size_t length, const ucp_am_recv_param_t *param)

static void ErrorCallback(void *arg, ucp_ep_h ep, ucs_status_t status) {
    const char* myName = "ErrorCallback";
    ConnStatus_t* connStatus = (ConnStatus_t*)arg;
    connStatus->isClosed = true;
    if (status == UCS_ERR_CONNECTION_RESET) {
        if (connStatus->isServer){
            tool::Logging(LOG_INFO, myName, "the client has closed the connection.\n");
        }
        else{
            tool::Logging(LOG_INFO, myName, "the server has shutdown the connection early.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        tool::Logging(LOG_ERROR, myName, "error handling callback was invoked with status %d (%s)\n",
            status, ucs_status_string(status));
    }
}

static void SendRecvCommonCallBack(void *request, ucs_status_t status, void *user_data) {
    const char* myName = "SendRecvCommonCallBack";
    Request_t *ctx;
    tool::Logging(LOG_COMM, myName, "request: %p, status: %d, user_data: %p\n", request, status, user_data);
    if (user_data == NULL) {
        tool::Logging(LOG_ERROR, myName, "user data is NULL.\n");
    } else {
        ctx = (Request_t *)user_data;
        if (ctx->type == 0) {
            tool::Logging(LOG_COMM, myName, "send callback is invoked, indicating the send request is completed.\n");
        } else {
            tool::Logging(LOG_COMM, myName, "recv callback is invoked, indicating the recv request is completed.\n");
        }
        ctx->complete = 1;
    }
}

static void SendCallBack(void *request, ucs_status_t status, void *user_data) {
    SendRecvCommonCallBack(request, status, user_data);
}

static void RecvCallBack(void *request, ucs_status_t status, size_t length, void *user_data) {
    SendRecvCommonCallBack(request, status, user_data);
}

static ucs_status_t ActiveMessageRecvCallback(void *arg, 
                            const void *header, size_t header_length,
                            void *data, size_t length,
                            const ucp_am_recv_param_t *param)
{
    const char* myName = "ActiveMessageRecvCallback";
    tool::Logging(LOG_COMM, myName, "received active message, header length %ld, data length %ld\n",
                                                    header_length, length);
    
    ActiveMessageDesc_t *amDataDesc = (ActiveMessageDesc_t *)arg;
    ucp_dt_iov_t        *iov        = amDataDesc->iov;
    size_t              iovCnt      = amDataDesc->iov_num 
                                    = header_length == 0 ? 1 : header_length / sizeof(size_t);
    void                *recv_buf   = amDataDesc->recv_buf; 
    
    amDataDesc->complete = 1; // mark the message has been received
    amDataDesc->recv_length = length;

    if (param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) {  // 2MB - 3MB
        /* Rendezvous request arrived, data contains an internal UCX descriptor, which has to be passed to ucp_am_recv_data_nbx() to initiate data transfer. */
        amDataDesc->is_rndv = 1;
        amDataDesc->desc    = data;
    }
    else {
        /* Eager request arrived, data should be available immediately */
        amDataDesc->is_rndv = 0;
    }
    tool::Logging(LOG_COMM, myName, "iovCnt = %zu\n", iovCnt);
    
    size_t offset = 0, idx = 0;
    for (idx = 0; idx < iovCnt; idx++) {
        iov[idx].length = header_length == 0 ? length : ((size_t*)header)[idx];

        if (amDataDesc->mem_type != UCS_MEMORY_TYPE_CUDA){
            iov[idx].buffer = (uint8_t*)recv_buf + offset; // todo
        }
        else {
            iov[idx].buffer = recv_buf;
        }

        if (!amDataDesc->is_rndv) {
            tool::Logging(LOG_COMM, myName, "copy data from the callback function to the variable amDataDesc (recv_buf).\n");
            switch (amDataDesc->mem_type){
            case UCS_MEMORY_TYPE_CUDA:
#ifdef USE_CUDA
                tool::Logging(myName, "copy data from host to device.\n");
                cudaMemcpy(iov[idx].buffer, UCS_PTR_BYTE_OFFSET(data, offset),
                    iov[idx].length, cudaMemcpyHostToDevice);
                break;
#endif 
            case UCS_MEMORY_TYPE_HOST:
                tool::Logging(LOG_COMM, myName, "copy data from host to host.\n");
                memcpy(iov[idx].buffer, UCS_PTR_BYTE_OFFSET(data, offset), 
                    iov[idx].length);
                break;
            default:
                tool::Logging(LOG_ERROR, myName, "unsupported memory type %d\n", amDataDesc->mem_type);
                return UCS_ERR_UNSUPPORTED;
            }
        }

        offset += iov[idx].length;
    } // move the data from the callback function to the variable amDataDesc (recv_buf)
    
    return amDataDesc->is_rndv ? UCS_INPROGRESS : UCS_OK;
}

static ucs_status_t RetrieveData(void *arg, 
                        const void *header, size_t header_length,
                        void *data, size_t length,
                        const ucp_am_recv_param_t *param){
    const char* myName = "RetrieveData";
    
    NewActiveMessageDesc_t *amDesc  = (NewActiveMessageDesc_t *)arg;
    ucp_dt_iov_t        *iov        = amDesc->iov;
    size_t              iovCnt      = amDesc->iov_num;
    // int                 amID        = amDesc->send_amID;

    // tool::Logging(LOG_COMM, myName, "received active message with amID %d, header length %ld, data length %ld\n", amID, header_length, length);
    tool::Logging(LOG_COMM, myName, "received active message, header length %ld, data length %ld\n", header_length, length);
    
    amDesc->complete = 1; // mark the message has been received

    if (param->recv_attr & UCP_AM_RECV_ATTR_FLAG_RNDV) {  // 2MB - 3MB
        /* Rendezvous request arrived, data contains an internal UCX descriptor, which has to be passed to ucp_am_recv_data_nbx() to initiate data transfer. */
        amDesc->is_rndv = 1;
        amDesc->desc    = data;
        return UCS_INPROGRESS;
    }
    else {
        /* Eager request arrived, data should be available immediately */
        amDesc->is_rndv = 0;
        tool::Logging(LOG_COMM, myName, "iovCnt = %zu\n", iovCnt);
        size_t offset = 0, idx = 0;
        for (idx = 0; idx < iovCnt; idx++) {
            tool::Logging(LOG_COMM, myName, "iov[%zu].length = %zu\n", idx, iov[idx].length);
            memcpy(iov[idx].buffer, UCS_PTR_BYTE_OFFSET(data, offset), iov[idx].length);
            offset += iov[idx].length;
        }
        return UCS_OK;
    }
}

/* ---- define common function ---- */

static void PrepareSingleIOV(ucp_dt_iov_t** iov, void* buffer, size_t length) {
    *iov = (ucp_dt_iov_t*)malloc(sizeof(ucp_dt_iov_t));
    (*iov)->buffer = buffer;
    (*iov)->length = length;
}

static void PrepareEmptyIOVList(ucp_dt_iov_t** iov, size_t iovcnt) {
    *iov = (ucp_dt_iov_t*)malloc(sizeof(ucp_dt_iov_t) * iovcnt);
}

static ucs_status_t Wait(void* request, Request_t* req_ctx, ucp_worker_h* dataWorker) {
    if (request == NULL) { // if operation was completed immediately
        return UCS_OK;
    }
    if (UCS_PTR_IS_ERR(request)) {
        return UCS_PTR_STATUS(request);
    }
    while (req_ctx->complete == 0) {
        ucp_worker_progress(*dataWorker);
    }
    return ucp_request_check_status(request);
}

static ucs_status_t SendData(ucp_dt_iov_t* iov, size_t iovCnt, size_t* header, size_t headerSize, uint64_t amID, ucp_worker_h* dataWorker, ucp_ep_h* ep, bool needReply = false, ucs_memory_type_t memType = UCS_MEMORY_TYPE_HOST, bool forcedEager = false) {
    const char* myName = "SendData";
    Request_t send_request_ctx = {.type = 0, .complete = 0};

    tool::Logging(LOG_COMM, myName, "iovCnt=%zu, headerSize=%zu, amID=%lu\n", iovCnt, headerSize, amID);

    ucp_request_param_t param;
    param.op_attr_mask      = UCP_OP_ATTR_FIELD_CALLBACK | 
                              UCP_OP_ATTR_FIELD_DATATYPE |
                              UCP_OP_ATTR_FIELD_USER_DATA;
    if (needReply | forcedEager) {
        param.op_attr_mask |= UCP_OP_ATTR_FIELD_FLAGS;
    }

    // param.op_attr_mask |= UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
    // param.flags        = UCP_AM_SEND_FLAG_RNDV;
    param.datatype          = (iovCnt == 1) ? ucp_dt_make_contig(1) : UCP_DATATYPE_IOV;
    param.user_data         = &send_request_ctx;
    param.cb.send           = (ucp_send_nbx_callback_t)SendRecvCommonCallBack;
    param.memory_type       = memType;
    param.flags             = UCP_AM_SEND_FLAG_REPLY;
    if (forcedEager) {
        param.flags         |= UCP_AM_SEND_FLAG_EAGER;
    }

    void*  msg              = (iovCnt == 1) ? iov[0].buffer : iov;
    size_t msg_length       = (iovCnt == 1) ? iov[0].length : iovCnt;
    Request_t* send_request = (Request_t*)ucp_am_send_nbx(
                                            *ep, amID, header, headerSize, 
                                            msg, msg_length, &param);
    
    tool::Logging(LOG_COMM, myName, "waiting for the send request to be completed.\n");
    ucs_status_t status     = Wait(send_request, &send_request_ctx, dataWorker); 
    if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName, "failed to send data: %s\n", ucs_status_string(status));
        return status;
    }
    else {
        tool::Logging(LOG_COMM, myName, "send request completed successfully.\n");
    }
    if (send_request != NULL) {
        ucp_request_free(send_request);
    }
    return status;
}

static ucs_status_t ReceiveData(ucp_dt_iov_t* iov, size_t* iovCnt, void* dataBuffer, uint64_t amID, ucp_worker_h* dataWorker, ucs_memory_type_t memType, bool* isClosed) {
    // iovCnt is set by ActiveMessageRecvCallback
    const char* myName = "ReceiveData";
    ucs_status_t status = UCS_OK;
    ActiveMessageDesc_t am_request_ctx = { .complete = 0, .is_rndv = 0, .mem_type = memType, .desc = NULL, 
                                           .iov = iov, .iov_num = 0, .recv_buf = dataBuffer, .recv_length = 0};
    Request_t recv_request_ctx = {.type = 1, .complete = 0};

    ucp_am_handler_param_t param1;
    param1.field_mask   = UCP_AM_HANDLER_PARAM_FIELD_ID |
                          UCP_AM_HANDLER_PARAM_FIELD_CB |
                          UCP_AM_HANDLER_PARAM_FIELD_ARG;
    param1.id           = amID; // todo: TEST_AM_ID
    param1.cb           = ActiveMessageRecvCallback;
    param1.arg          = &am_request_ctx;
    status = ucp_worker_set_am_recv_handler(*dataWorker, &param1);
    if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName, "failed to set am handler.\n");
        return status;
    }

    tool::Logging(LOG_COMM, myName, "waiting for the client to send a message.\n");
    while (!am_request_ctx.complete) { // waiting ActiveMessageRecvCallback() to be invoked
        if (isClosed != NULL && *isClosed) {
            tool::Logging(LOG_ERROR, myName, "the connection has been closed.\n");
            return UCS_ERR_CONNECTION_RESET;
        }
        ucp_worker_progress(*dataWorker);
    }

    *iovCnt = am_request_ctx.iov_num;
    size_t msg_length = am_request_ctx.recv_length;
    if (!am_request_ctx.is_rndv) {
        tool::Logging(LOG_COMM, myName, "Eager request has arrived\n");
    }
    else {
        tool::Logging(LOG_COMM, myName, "Rendezvous request has arrived\n");

        ucp_request_param_t param2;
        param2.op_attr_mask     = UCP_OP_ATTR_FIELD_CALLBACK |
                                  UCP_OP_ATTR_FIELD_DATATYPE |
                                  UCP_OP_ATTR_FIELD_USER_DATA|
                                  UCP_OP_ATTR_FIELD_MEMORY_TYPE;
        param2.op_attr_mask    |= UCP_OP_ATTR_FLAG_NO_IMM_CMPL;
        // param2.datatype     = (*iovCnt == 1) ? ucp_dt_make_contig(1) : UCP_DATATYPE_IOV;
        param2.datatype         = ucp_dt_make_contig(1);
        param2.user_data        = &recv_request_ctx;
        param2.cb.recv_am       = (ucp_am_recv_data_nbx_callback_t)RecvCallBack;
        param2.memory_type      = memType;
        Request_t* rndv_request = (Request_t*)ucp_am_recv_data_nbx(*dataWorker,
                                              am_request_ctx.desc,
                                              am_request_ctx.recv_buf, am_request_ctx.recv_length, //recv data
                                             &param2);

        status = Wait(rndv_request, &recv_request_ctx, dataWorker); 
        if (status != UCS_OK) {
            tool::Logging(LOG_ERROR, myName, "ucp_am_recv_data_nbx failed: %s\n", ucs_status_string(status));
        }
        else {
            tool::Logging(LOG_COMM, myName, "ucp_am_recv_data_nbx completed successfully.\n");
        }
        ucp_request_free(rndv_request);
    }
    return status;
}

static ucs_status_t RegisterHandler(uint16_t amID, ucp_am_recv_callback_t cb, ucp_worker_h worker, void* arg) {
    const char* myName  = "RegisterHandler";
    ucs_status_t status = UCS_OK;

    ucp_am_handler_param_t param;
    param.field_mask    = UCP_AM_HANDLER_PARAM_FIELD_ID |
                          UCP_AM_HANDLER_PARAM_FIELD_CB |
                          UCP_AM_HANDLER_PARAM_FIELD_ARG;
    param.id            = amID;
    param.cb            = cb;
    param.arg           = arg;
    status              = ucp_worker_set_am_recv_handler(worker, &param);
    if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName, "failed to set am handler for req#%d: %s\n", amID, ucs_status_string(status));
    }
    return status;
}

#endif
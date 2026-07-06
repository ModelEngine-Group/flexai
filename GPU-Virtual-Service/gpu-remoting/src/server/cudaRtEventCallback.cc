/* CUDA Event API */
#include "../../include/serverEndpoint.h"

static const char* myName = "CUDARuntimeEventHandle";

DEFINE_SERVER_AM_CALLBACK(cudaEventCreateHandle) {
    tool::Logging(myName, "CUDA_EVENT_CREATE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        virAddr     = reqBuf.Pop<uint64_t>();
    cudaEvent_t     event       = NULL;
    cudaError_t     exit_code   = cudaEventCreate(&event);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaEventCreate success, event ptr: %p, size=%d\n", event, sizeof(uint64_t));
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, event);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDA_EVENT_CREATE);
            event = (cudaEvent_t)serverEp->GetHandleVirAddr(event, CUDA_EVENT_CREATE);
            resBuf.Push64BitPointer(event);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaEventCreate failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaEventCreateWithFlagsHandle) {
    tool::Logging(myName, "CUDA_EVENT_CREATE_WITH_FLAGS\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        virAddr     = reqBuf.Pop<uint64_t>();
    cudaEvent_t     event       = NULL;
    unsigned int    flags       = reqBuf.Pop<uint>();
    cudaError_t     exit_code   = cudaEventCreateWithFlags(&event,flags);
    if (exit_code == cudaSuccess) {
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, event);
            tool::Logging(LOG_DEBUG, myName, "cudaEventCreateWithFlags success, event ptr: %p(virAddr=%p, pid=%d, ttid=%d), flags=%u\n", event, virAddr, serverEp->clientPID_, reqBuf.GetThreadID(), flags);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDA_EVENT_CREATE_WITH_FLAGS);
            void* virtAddr = (cudaEvent_t)serverEp->GetHandleVirAddr(event, CUDA_EVENT_CREATE_WITH_FLAGS);
            resBuf.Push64BitPointer(virtAddr);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
            tool::Logging(LOG_DEBUG, myName, "cudaEventCreateWithFlags success, event ptr: %p(new virAddr=%p, pid=%d, ttid=%d), flags=%u\n", event, virtAddr, serverEp->clientPID_, reqBuf.GetThreadID(), flags);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaEventCreateWithFlags failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaEventRecordHandle){
    tool::Logging(myName, "CUDA_EVENT_Record\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        eventVirAddr= reqBuf.Pop<uint64_t>();
    cudaEvent_t     event       = (cudaEvent_t)serverEp->GetHandle(eventVirAddr);
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    cudaError_t     exit_code   = cudaEventRecord(event,stream);

    if (exit_code == cudaSuccess) {
#ifdef GV_HANDLE
        Handle_t* handle = serverEp->GetHandleInfo(eventVirAddr);
        if (handle != NULL) {
            handle->stream = (uint64_t)stream;
        }
#endif

        tool::Logging(myName, "cudaEventRecord success, eventPtr=%p, streamPtr=%p(isCommStream=%d)\n",event,stream,stream == serverEp->commStream_);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaEventRecord failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaEventQueryHandle) {
    tool::Logging(myName, "CUDA_EVENT_QUERY\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        eventVirAddr= reqBuf.Pop<uint64_t>();
    cudaEvent_t     event       = (cudaEvent_t)serverEp->GetHandle(eventVirAddr);
    cudaError_t     exit_code   = cudaSuccess;
    if (event == NULL) {
        tool::Logging(LOG_ERROR, myName, "cudaEventQuery warning: eventPtr is NULL(virAddr=%p, pid=%d, ttid=%d)\n", eventVirAddr, serverEp->clientPID_, reqBuf.GetThreadID());
    }
    else {
        exit_code = cudaEventQuery(event);
    }

    RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
    resBuf.PushRequestType(CUDA_EVENT_QUERY);
    resBuf.Push(exit_code);
    serverEp->SendResponse(&resBuf, &param->reply_ep);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaEventQuery success, eventPtr=%p\n",event);
    }
    else if (exit_code == cudaErrorNotReady) {
        tool::Logging(myName, "cudaEventQuery success, eventPtr=%p, cudaErrorNotReady(any captured work is incomplete)\n",event);
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaEventQuery failed for eventPtr=%p: %s\n", event, cudaGetErrorName(exit_code));
    }
    return UCS_OK;
}

DEFINE_SERVER_AM_CALLBACK(cudaEventDestroyHandle) {
    tool::Logging(myName, "CUDA_EVENT_DESTROY\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        eventVirAddr= reqBuf.Pop<uint64_t>();
    cudaEvent_t     event       = (cudaEvent_t)serverEp->GetHandle(eventVirAddr, true);
    if (event == NULL) {
        tool::Logging(LOG_ERROR, myName, "cudaEventDestroy success, eventPtr=%p(virAddr=%p, pid=%d, ttid=%d)\n",event, eventVirAddr, serverEp->clientPID_, reqBuf.GetThreadID());
        return UCS_OK;
    }
    cudaError_t     exit_code   = cudaEventDestroy(event);
    if (exit_code == cudaSuccess) {
        tool::Logging(LOG_DEBUG, myName, "cudaEventDestroy success, eventPtr=%p(virAddr=%p, pid=%d, ttid=%d)\n",event, eventVirAddr, serverEp->clientPID_, reqBuf.GetThreadID());
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaEventDestroy failed: %s, eventPtr=%p\n", cudaGetErrorName(exit_code), event);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaEventElapsedTimeHandle) {
    tool::Logging(myName, "CUDA_EVENT_ELAPSED_TIME\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    float           ms          = 0;
    cudaEvent_t     start       = (cudaEvent_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaEvent_t     end         = (cudaEvent_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaError_t     exit_code   = cudaEventElapsedTime(&ms,start,end);    
    if (exit_code == cudaSuccess || exit_code == cudaErrorNotReady) {
        tool::Logging(myName, "cudaEventElapsedTime: %s, startPtr=%p, endPtr=%p, ms=%f\n",cudaGetErrorName(exit_code), start,end,ms);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_EVENT_ELAPSED_TIME);
        resBuf.Push(exit_code);
        resBuf.Push(ms);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaEventElapsedTime failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}
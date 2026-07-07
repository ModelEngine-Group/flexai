#include "../../include/serverEndpoint.h"

static const char* myName = "cuBlasHandle";

DEFINE_SERVER_AM_CALLBACK(cublasCreate_v2Handle) {
    tool::Logging(myName, "CUBLAS_CREATE_V2\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    cublasHandle_t      handle      = NULL;
    cublasStatus_t      exit_code   = cublasCreate_v2(&handle);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasCreate_v2 success, handle = %p\n", handle);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, handle);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUBLAS_CREATE_V2);
            handle = (cublasHandle_t)serverEp->GetHandleVirAddr(handle, CUBLAS_CREATE_V2);
            resBuf.Push64BitPointer(handle);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasCreate_v2 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasSgemm_v2Handle) {
    tool::Logging(myName, "CUBLAS_SGEMM_V2\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasOperation_t   transa      = reqBuf.Pop<cublasOperation_t>();
    cublasOperation_t   transb      = reqBuf.Pop<cublasOperation_t>();
    int                 m           = reqBuf.Pop<int>();
    int                 n           = reqBuf.Pop<int>();
    int                 k           = reqBuf.Pop<int>();
    const float*        alpha       = reqBuf.AssignAddr<const float>();
    const float*        A           = (const float*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    int                 lda         = reqBuf.Pop<int>();
    const float*        B           = (const float*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    int                 ldb         = reqBuf.Pop<int>();
    const float*        beta        = reqBuf.AssignAddr<const float>();
    float*              C           = (float*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    int                 ldc         = reqBuf.Pop<int>();
    cublasStatus_t      exit_code   = cublasSgemm_v2(handle, transa, transb, m, n, k, 
                                                     alpha, A, lda, B, ldb, beta, C, ldc);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasSgemm_v2 success, handle=%p\n", handle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasSgemm_v2 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasSgemmStridedBatchedHandle) {
    tool::Logging(myName, "CUBLAS_SGEMM_STRIDED_BATCHED\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasOperation_t   transa      = reqBuf.Pop<cublasOperation_t>();
    cublasOperation_t   transb      = reqBuf.Pop<cublasOperation_t>();
    int                 m           = reqBuf.Pop<int>();
    int                 n           = reqBuf.Pop<int>();
    int                 k           = reqBuf.Pop<int>();
    const float*        alpha       = reqBuf.AssignAddr<const float>();
    const float*        A           = (const float*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    int                 lda         = reqBuf.Pop<int>();
    long long int       strideA     = reqBuf.Pop<long long int>();
    const float*        B           = (const float*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    int                 ldb         = reqBuf.Pop<int>();
    long long int       strideB     = reqBuf.Pop<long long int>();
    const float*        beta        = reqBuf.AssignAddr<const float>();
    float*              C           = (float*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    int                 ldc         = reqBuf.Pop<int>();
    long long int       strideC     = reqBuf.Pop<long long int>();
    int                 batchCount  = reqBuf.Pop<int>();
    cublasStatus_t      exit_code   = cublasSgemmStridedBatched(handle, transa, transb, m, n, k, 
                                                                alpha, A, lda, strideA, B, ldb, strideB, beta, 
                                                                C, ldc, strideC, batchCount);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasSgemmStridedBatched success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasSgemmStridedBatched failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasDestroy_v2Handle) {
    tool::Logging(myName, "CUBLAS_DESTROY_V2\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cublasStatus_t      exit_code   = cublasDestroy_v2(handle);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasDestroy_v2 success, handle = %p\n", handle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasDestroy_v2 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasSetStream_v2Handle) {
    tool::Logging(myName, "CUBLAS_SET_STREAM_V2\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t        stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    cublasStatus_t      exit_code   = cublasSetStream_v2(handle, stream);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasSetStream_v2 success, handle=%p, stream=%p\n", handle, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasSetStream_v2 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasSetWorkspace_v2Handle) {
    tool::Logging(myName, "CUBLAS_SET_WORKSPACE_V2\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*               workspace   = serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t              size        = reqBuf.Pop<size_t>();
    cublasStatus_t      exit_code   = cublasSetWorkspace_v2(handle, workspace, size);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasSetWorkspace_v2 success, handle=%p\n", handle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasSetWorkspace_v2 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasSetMathModeHandle) {
    tool::Logging(myName, "CUBLAS_SET_MATH_MODE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasMath_t        mode        = reqBuf.Pop<cublasMath_t>();
    cublasStatus_t      exit_code   = cublasSetMathMode(handle, mode);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasSetMathMode success, handle=%p\n", handle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasSetMathMode failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasGetMathModeHandle) {
    tool::Logging(myName, "CUBLAS_GET_MATH_MODE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasHandle_t      handle      = (cublasHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasMath_t        mode;
    cublasStatus_t      exit_code   = cublasGetMathMode(handle, &mode);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasGetMathMode success\n");
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUBLAS_GET_MATH_MODE);
        resBuf.Push(mode);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasGetMathMode failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}
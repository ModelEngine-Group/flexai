#include "../../include/serverEndpoint.h"

static const char* myName = "cuBlasLtHandle";

DEFINE_SERVER_AM_CALLBACK(cublasLtCreateHandle) {
    tool::Logging(myName, "CUBLASLT_CREATE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    cublasLtHandle_t    lightHandle = NULL;
    cublasStatus_t      exit_code   = cublasLtCreate(&lightHandle);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtCreate success, lightHandle = %p\n", lightHandle);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, lightHandle);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUBLASLT_CREATE);
            lightHandle = (cublasLtHandle_t)serverEp->GetHandleVirAddr(lightHandle, CUBLASLT_CREATE);
            resBuf.Push64BitPointer(lightHandle);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtCreate failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtDestroyHandle) {
    tool::Logging(myName, "CUBLASLT_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtHandle_t    lightHandle = (cublasLtHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cublasStatus_t      exit_code   = cublasLtDestroy(lightHandle);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtDestroy success, lightHandle = %p\n", lightHandle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtDestroy failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulDescCreateHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULDESC_CREATE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    cublasLtMatmulDesc_t matmulDesc;
    cublasComputeType_t computeType = reqBuf.Pop<cublasComputeType_t>();
    cudaDataType_t      scaleType   = reqBuf.Pop<cudaDataType_t>();
    cublasStatus_t      exit_code   = cublasLtMatmulDescCreate(&matmulDesc, computeType, scaleType);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulDescCreate success, matmulDesc = %p\n", matmulDesc);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, matmulDesc);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUBLASLT_MATMULDESC_CREATE);
            matmulDesc = (cublasLtMatmulDesc_t)serverEp->GetHandleVirAddr(matmulDesc, CUBLASLT_MATMULDESC_CREATE);
            resBuf.Push64BitPointer(matmulDesc);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulDescCreate failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulDescDestroyHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULDESC_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtMatmulDesc_t matmulDesc = (cublasLtMatmulDesc_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cublasStatus_t      exit_code   = cublasLtMatmulDescDestroy(matmulDesc);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulDescDestroy success, matmulDesc = %p\n", matmulDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulDescDestroy failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulDescSetAttributeHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULDESC_SETATTRIBUTE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtMatmulDesc_t matmulDesc = (cublasLtMatmulDesc_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatmulDescAttributes_t attr = reqBuf.Pop<cublasLtMatmulDescAttributes_t>();
    const void*         buf         = reqBuf.AssignAddrForAll<const void>();
    size_t              sizeInBytes = reqBuf.Pop<size_t>();
    if (attr == CUBLASLT_MATMUL_DESC_BIAS_POINTER || attr == CUBLASLT_MATMUL_DESC_EPILOGUE_AUX_POINTER 
        // || CUBLASLT_MATMUL_DESC_A_SCALE_POINTER || CUBLASLT_MATMUL_DESC_B_SCALE_POINTER || CUBLASLT_MATMUL_DESC_C_SCALE_POINTER || ... cuBlasLt v12.5 have more attributes related to devPtr
    ) {
        uint64_t*       virtAddrPtr = (uint64_t*)buf;
        void*           devPtr      = serverEp->GetDevPtr(*virtAddrPtr);
        *virtAddrPtr                = (uint64_t)devPtr;
    }

    cublasStatus_t      exit_code   = cublasLtMatmulDescSetAttribute(matmulDesc, attr, buf, sizeInBytes);


    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulDescSetAttribute success, desc = %p, attr = %d, buf = %p(val = %p), sizeInBytes = %lu\n", matmulDesc, attr, buf, *(uint64_t*)buf, sizeInBytes);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulDescSetAttribute failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatrixLayoutCreateHandle) {
    tool::Logging(myName, "CUBLASLT_MATRIX_LAYOUT_CREATE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    cublasLtMatrixLayout_t layout;
    cudaDataType_t      dataType    = reqBuf.Pop<cudaDataType_t>();
    uint64_t            rows        = reqBuf.Pop<uint64_t>();
    uint64_t            cols        = reqBuf.Pop<uint64_t>();
    int64_t             ld          = reqBuf.Pop<int64_t>();
    cublasStatus_t      exit_code   = cublasLtMatrixLayoutCreate(&layout, dataType, rows, cols, ld);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatrixLayoutCreate success, layout = %p\n", layout);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, layout);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUBLASLT_MATRIX_LAYOUT_CREATE);
            layout = (cublasLtMatrixLayout_t)serverEp->GetHandleVirAddr(layout, CUBLASLT_MATRIX_LAYOUT_CREATE);
            resBuf.Push64BitPointer(layout);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatrixLayoutCreate failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatrixLayoutDestroyHandle) {
    tool::Logging(myName, "CUBLASLT_MATRIX_LAYOUT_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtMatrixLayout_t layout   = (cublasLtMatrixLayout_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cublasStatus_t      exit_code   = cublasLtMatrixLayoutDestroy(layout);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatrixLayoutDestroy success, layout = %p\n", layout);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatrixLayoutDestroy failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatrixLayoutSetAttributeHandle) {
    tool::Logging(myName, "CUBLASLT_MATRIX_LAYOUT_SETATTRIBUTE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtMatrixLayout_t layout = (cublasLtMatrixLayout_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayoutAttribute_t attr = reqBuf.Pop<cublasLtMatrixLayoutAttribute_t>();
    const void*         buf         = reqBuf.AssignAddrForAll<const void>();
    size_t              sizeInBytes = reqBuf.Pop<size_t>();
    cublasStatus_t      exit_code   = cublasLtMatrixLayoutSetAttribute(layout, attr, buf, sizeInBytes);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatrixLayoutSetAttribute success: layout = %p, attr = %d, buf = %p(val = %p), sizeInBytes = %lu\n", layout, attr, buf, *(uint64_t*)buf, sizeInBytes);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatrixLayoutSetAttribute failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulPreferenceCreateHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULPREFERENCE_CREATE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    cublasLtMatmulPreference_t pref;
    cublasStatus_t      exit_code   = cublasLtMatmulPreferenceCreate(&pref);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulPreferenceCreate success, pref = %p\n", pref);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, pref);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUBLASLT_MATMULPREFERENCE_CREATE);
            pref = (cublasLtMatmulPreference_t)serverEp->GetHandleVirAddr(pref, CUBLASLT_MATMULPREFERENCE_CREATE);
            resBuf.Push64BitPointer(pref);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulPreferenceCreate failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulPreferenceDestroyHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULPREFERENCE_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtMatmulPreference_t pref = (cublasLtMatmulPreference_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cublasStatus_t      exit_code   = cublasLtMatmulPreferenceDestroy(pref);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulPreferenceDestroy success, pref = %p\n", pref);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulPreferenceDestroy failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulPreferenceSetAttributeHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULPREFERENCE_SETATTRIBUTE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtMatmulPreference_t pref = (cublasLtMatmulPreference_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatmulPreferenceAttributes_t attr = reqBuf.Pop<cublasLtMatmulPreferenceAttributes_t>();
    const void*         buf         = reqBuf.AssignAddrForAll<const void>();
    size_t              sizeInBytes = reqBuf.Pop<size_t>();
    cublasStatus_t      exit_code   = cublasLtMatmulPreferenceSetAttribute(pref, attr, buf, sizeInBytes);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulPreferenceSetAttribute success: pref = %p, attr = %d, buf = %p(val = %p), sizeInBytes = %lu\n", pref, attr, buf, *(uint64_t*)buf, sizeInBytes);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulPreferenceSetAttribute failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulAlgoGetHeuristicHandle) {
    tool::Logging(myName, "CUBLASLT_MATMULALGO_GETHEURISTIC\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtHandle_t    lightHandle = (cublasLtHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatmulDesc_t matmulDesc = (cublasLtMatmulDesc_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t layoutA  = (cublasLtMatrixLayout_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t layoutB  = (cublasLtMatrixLayout_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t layoutC  = (cublasLtMatrixLayout_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t layoutD  = (cublasLtMatrixLayout_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatmulPreference_t pref = (cublasLtMatmulPreference_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                reqAlgoCount = reqBuf.Pop<int>();
    cublasLtMatmulHeuristicResult_t heuristicResult[reqAlgoCount];
    int                retAlgoCount = 0;
    cublasStatus_t      exit_code   = cublasLtMatmulAlgoGetHeuristic(lightHandle, matmulDesc, layoutA, layoutB, layoutC, layoutD, pref, reqAlgoCount, heuristicResult, &retAlgoCount);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmulAlgoGetHeuristic success: retAlgoCount = %d, lightHandle = %p, matmulDesc = %p, layoutA = %p, layoutB = %p, layoutC = %p, layoutD = %p, pref = %p\n", retAlgoCount, lightHandle, matmulDesc, layoutA, layoutB, layoutC, layoutD, pref);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUBLASLT_MATMULALGO_GETHEURISTIC);
        resBuf.Push(retAlgoCount);
        resBuf.Push(heuristicResult, retAlgoCount);
        // heuristicResult contains cublasLtMatmulAlgo_t, which is a structure that describes the algorithm, and can be trivially serialized and later restored for use with the same version of cuBLAS library to save on selecting the right configuration again.
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmulAlgoGetHeuristic failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cublasLtMatmulHandle) {
    tool::Logging(myName, "CUBLASLT_MATMUL\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cublasLtHandle_t    lightHandle = (cublasLtHandle_t)        serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatmulDesc_t compDesc   = (cublasLtMatmulDesc_t)    serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const float*        alpha       = reqBuf.AssignAddr<const float>(); // default: host pointer, set by CUBLASLT_MATMUL_DESC_POINTER_MODE
    const void*         A           = (const void*)             serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t Adesc    = (cublasLtMatrixLayout_t)  serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*         B           = (const void*)             serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t Bdesc    = (cublasLtMatrixLayout_t)  serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const float*        beta        = reqBuf.AssignAddr<const float>();
    const void*         C           = (const void*)             serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t Cdesc    = (cublasLtMatrixLayout_t)  serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*               D           = (void*)                   serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cublasLtMatrixLayout_t Ddesc    = (cublasLtMatrixLayout_t)  serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cublasLtMatmulAlgo_t*  algo     = reqBuf.AssignAddr<cublasLtMatmulAlgo_t>();
    void*               workspace   = (void*)                   serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t              workspaceSizeInBytes = reqBuf.Pop<size_t>();
    cudaStream_t        stream      = (cudaStream_t)            serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    cublasStatus_t      exit_code   = cublasLtMatmul(lightHandle, compDesc, alpha, A, Adesc, B, Bdesc, beta, C, Cdesc, D, Ddesc, algo, workspace, workspaceSizeInBytes, stream);
    if (exit_code == CUBLAS_STATUS_SUCCESS) {
        tool::Logging(myName, "cublasLtMatmul success\n");
        // cudaError_t exit_code_o = cudaDeviceSynchronize();
        // if (exit_code_o != cudaSuccess) {
        //     tool::Logging(LOG_ERROR, myName_, "cublasLtMatmul after cudaDeviceSynchronize failed: %d(%s)\n", exit_code_o, cudaGetErrorString(exit_code_o));
        //     exit(EXIT_FAILURE);
        // }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cublasLtMatmul failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}
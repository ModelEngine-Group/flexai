#include "../../include/hook/hook.h"

cublasStatus_t cublasLtCreate(cublasLtHandle_t *lightHandle) {
    const char* func_name = "cublasLtCreate";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtHandle_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtCreate"));
    
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_CREATE);
    *lightHandle = NULL;
    reqBuf.Push(lightHandle);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(lightHandle);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(lightHandle);
}

cublasStatus_t cublasLtDestroy(cublasLtHandle_t lightHandle) {
    const char* func_name = "cublasLtDestroy";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtHandle_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtDestroy"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_DESTROY);
    reqBuf.Push64BitPointer(lightHandle);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;
    
    //return func_entry(lightHandle);
}

cublasStatus_t cublasLtMatmulDescCreate(cublasLtMatmulDesc_t *matmulDesc,
                                        cublasComputeType_t computeType, cudaDataType_t scaleType) {
    const char* func_name = "cublasLtMatmulDescCreate";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatmulDesc_t *, cublasComputeType_t, cudaDataType_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulDescCreate"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(cublasComputeType_t) + sizeof(cudaDataType_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULDESC_CREATE);
    *matmulDesc = NULL;
    reqBuf.Push(matmulDesc);
    reqBuf.Push(computeType);
    reqBuf.Push(scaleType);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(matmulDesc);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(matmulDesc, computeType, scaleType);
}

cublasStatus_t cublasLtMatmulDescDestroy(cublasLtMatmulDesc_t matmulDesc) {
    const char* func_name = "cublasLtMatmulDescDestroy";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatmulDesc_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulDescDestroy"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULDESC_DESTROY);
    reqBuf.Push64BitPointer(matmulDesc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;
    
    //return func_entry(matmulDesc);
}

cublasStatus_t cublasLtMatmulDescSetAttribute(cublasLtMatmulDesc_t matmulDesc,
            cublasLtMatmulDescAttributes_t attr, const void *buf, size_t sizeInBytes) {
    const char* func_name = "cublasLtMatmulDescSetAttribute";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatmulDesc_t, cublasLtMatmulDescAttributes_t, const void *, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulDescSetAttribute"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cublasLtMatmulDescAttributes_t) + sizeof(size_t)+sizeInBytes + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULDESC_SETATTRIBUTE);
    reqBuf.Push64BitPointer(matmulDesc);
    reqBuf.Push(attr);
    reqBuf.PushConst((uint8_t*)buf, sizeInBytes); 
    reqBuf.Push(sizeInBytes);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(matmulDesc, attr, buf, sizeInBytes);
}

cublasStatus_t cublasLtMatrixLayoutCreate(cublasLtMatrixLayout_t *matLayout, cudaDataType type, 
                                          uint64_t rows, uint64_t cols, int64_t ld) {
    const char* func_name = "cublasLtMatrixLayoutCreate";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatrixLayout_t *, cudaDataType, uint64_t, uint64_t, int64_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatrixLayoutCreate"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudaDataType) + sizeof(uint64_t) + sizeof(uint64_t) + sizeof(int64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATRIX_LAYOUT_CREATE);
    *matLayout = NULL;
    reqBuf.Push(matLayout);
    reqBuf.Push(type);
    reqBuf.Push(rows);
    reqBuf.Push(cols);
    reqBuf.Push(ld);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(matLayout);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(matLayout, type, rows, cols, ld);
}

cublasStatus_t cublasLtMatrixLayoutDestroy(cublasLtMatrixLayout_t matLayout) {
    const char* func_name = "cublasLtMatrixLayoutDestroy";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatrixLayout_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatrixLayoutDestroy"));
    tool::Logging(LOG_DEBUG, func_name, "matLayout: %p\n", matLayout);
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATRIX_LAYOUT_DESTROY);
    reqBuf.Push64BitPointer(matLayout);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;
    
    //return func_entry(matLayout);
}

cublasStatus_t cublasLtMatrixLayoutSetAttribute(cublasLtMatrixLayout_t matLayout, 
                                                cublasLtMatrixLayoutAttribute_t attr,
                                                const void *buf, size_t sizeInBytes) {
    const char* func_name = "cublasLtMatrixLayoutSetAttribute";
    HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatrixLayout_t, cublasLtMatrixLayoutAttribute_t, const void *, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatrixLayoutSetAttribute"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cublasLtMatrixLayoutAttribute_t) + sizeof(size_t)+sizeInBytes + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATRIX_LAYOUT_SETATTRIBUTE);
    reqBuf.Push64BitPointer(matLayout);
    reqBuf.Push(attr);
    reqBuf.PushConst((uint8_t*)buf, sizeInBytes);
    reqBuf.Push(sizeInBytes);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(matLayout, attr, buf, sizeInBytes);
}

cublasStatus_t cublasLtMatmulPreferenceCreate(cublasLtMatmulPreference_t *pref) {
    const char* func_name = "cublasLtMatmulPreferenceCreate";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatmulPreference_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulPreferenceCreate"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULPREFERENCE_CREATE);
    *pref = NULL;
    reqBuf.Push(pref);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(pref);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(pref);
}

cublasStatus_t cublasLtMatmulPreferenceDestroy(cublasLtMatmulPreference_t pref) {
    const char* func_name = "cublasLtMatmulPreferenceDestroy";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtMatmulPreference_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulPreferenceDestroy"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULPREFERENCE_DESTROY);
    reqBuf.Push64BitPointer(pref);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(pref);
}

cublasStatus_t cublasLtMatmulPreferenceSetAttribute(cublasLtMatmulPreference_t pref, 
               cublasLtMatmulPreferenceAttributes_t attr, const void *buf, size_t sizeInBytes) {
    const char* func_name = "cublasLtMatmulPreferenceSetAttribute";
	HookLog(func_name);
    using func_ptr =
        cublasStatus_t (*)(cublasLtMatmulPreference_t, cublasLtMatmulPreferenceAttributes_t, const void *, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulPreferenceSetAttribute"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cublasLtMatmulPreferenceAttributes_t) + sizeof(size_t)+sizeInBytes + sizeInBytes);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULPREFERENCE_SETATTRIBUTE);
    reqBuf.Push64BitPointer(pref);
    reqBuf.Push(attr);
    reqBuf.PushConst((uint8_t*)buf, sizeInBytes);
    reqBuf.Push(sizeInBytes);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;
    
    //return func_entry(pref, attr, buf, sizeInBytes);
}

cublasStatus_t cublasLtMatmulAlgoGetHeuristic(
    cublasLtHandle_t lightHandle, cublasLtMatmulDesc_t operationDesc, cublasLtMatrixLayout_t Adesc,
    cublasLtMatrixLayout_t Bdesc, cublasLtMatrixLayout_t Cdesc, cublasLtMatrixLayout_t Ddesc,
    cublasLtMatmulPreference_t preference, int requestedAlgoCount,
    cublasLtMatmulHeuristicResult_t heuristicResultsArray[], int *returnAlgoCount) {
    const char* func_name = "cublasLtMatmulAlgoGetHeuristic";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasLtHandle_t, cublasLtMatmulDesc_t, cublasLtMatrixLayout_t,
                                        cublasLtMatrixLayout_t, cublasLtMatrixLayout_t, cublasLtMatrixLayout_t,
                                        cublasLtMatmulPreference_t, int, cublasLtMatmulHeuristicResult_t[], int *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmulAlgoGetHeuristic"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) * 7 + sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMULALGO_GETHEURISTIC);
    reqBuf.Push64BitPointer(lightHandle);
    reqBuf.Push64BitPointer(operationDesc);
    reqBuf.Push64BitPointer(Adesc);
    reqBuf.Push64BitPointer(Bdesc);
    reqBuf.Push64BitPointer(Cdesc);
    reqBuf.Push64BitPointer(Ddesc);
    reqBuf.Push64BitPointer(preference);
    reqBuf.Push(requestedAlgoCount);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(returnAlgoCount);
    resBuf.Push(heuristicResultsArray, requestedAlgoCount);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;
    
    //return func_entry(lightHandle, operationDesc, Adesc, Bdesc, Cdesc, Ddesc, preference, requestedAlgoCount, heuristicResultsArray, returnAlgoCount);
}

cublasStatus_t cublasLtMatmul(
    cublasLtHandle_t lightHandle, cublasLtMatmulDesc_t computeDesc, const void *alpha, const void *A,
    cublasLtMatrixLayout_t Adesc, const void *B, cublasLtMatrixLayout_t Bdesc, const void *beta, const void *C,
    cublasLtMatrixLayout_t Cdesc, void *D, cublasLtMatrixLayout_t Ddesc, const cublasLtMatmulAlgo_t *algo,
    void *workspace, size_t workspaceSizeInBytes, cudaStream_t stream) {
    const char* func_name = "cublasLtMatmul";
	HookLog(func_name);
    using func_ptr =
        cublasStatus_t (*)(cublasLtHandle_t, cublasLtMatmulDesc_t, const void *, const void *, cublasLtMatrixLayout_t,
                           const void *, cublasLtMatrixLayout_t, const void *, const void *, cublasLtMatrixLayout_t,
                           void *, cublasLtMatrixLayout_t, const cublasLtMatmulAlgo_t *, void *, size_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasLtMatmul"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t)*2 + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*8 + sizeof(size_t)+sizeof(const cublasLtMatmulAlgo_t) + sizeof(uint64_t) + sizeof(size_t) + sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLASLT_MATMUL);
    reqBuf.Push64BitPointer(lightHandle);
    reqBuf.Push64BitPointer(computeDesc);
    reqBuf.PushConst((const float*)alpha); //todo: device or host
    reqBuf.Push64BitPointer(A); // device pointer
    reqBuf.Push64BitPointer(Adesc);
    reqBuf.Push64BitPointer(B); // device pointer
    reqBuf.Push64BitPointer(Bdesc);
    reqBuf.PushConst((const float*)beta); //todo: device or host
    reqBuf.Push64BitPointer(C); // device pointer
    reqBuf.Push64BitPointer(Cdesc);
    reqBuf.Push64BitPointer(D); // device pointer
    reqBuf.Push64BitPointer(Ddesc);
    reqBuf.PushConst(algo);
    reqBuf.Push64BitPointer(workspace); // device pointer
    reqBuf.Push(workspaceSizeInBytes);
    reqBuf.Push64BitPointer(stream);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(lightHandle, computeDesc, alpha, A, Adesc, B, Bdesc, beta, C, Cdesc, D, Ddesc, algo, workspace,workspaceSizeInBytes, stream);
}
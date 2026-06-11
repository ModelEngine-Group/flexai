#include "../../include/hook/hook.h"


cublasStatus_t cublasCreate_v2(cublasHandle_t *handle) {
    const char* func_name = "cublasCreate_v2";
    HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, func_name));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_CREATE_V2);
    *handle = NULL;
    reqBuf.Push(handle);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(handle);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle);
}

cublasStatus_t cublasSgemm_v2(cublasHandle_t handle, 
                              cublasOperation_t transa, cublasOperation_t transb, 
                              int m, int n, int k,
                              const float *alpha, const float *A, int lda, 
                              const float *B, int ldb, const float *beta, 
                              float *C, int ldc) {
    const char* func_name = "cublasSgemm_v2";
	HookLog(func_name);
    using func_ptr =
        cublasStatus_t (*)(cublasHandle_t, cublasOperation_t, cublasOperation_t, int, int, int, const float *,
                           const float *, int, const float *, int, const float *, float *, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasSgemm_v2"));

    // printf("alpha: %p, beta: %p\n", alpha, beta);

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cublasOperation_t) + sizeof(cublasOperation_t) + 3 * sizeof(int) + 2 * (sizeof(size_t)+sizeof(const float)) + 3 * sizeof(uint64_t) + 3 * sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_SGEMM_V2);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(transa);
    reqBuf.Push(transb);
    reqBuf.Push(m);
    reqBuf.Push(n);
    reqBuf.Push(k);
    reqBuf.PushConst(alpha);
    reqBuf.Push64BitPointer(A);     // device pointer
    reqBuf.Push(lda);
    reqBuf.Push64BitPointer(B);     // device pointer
    reqBuf.Push(ldb);
    reqBuf.PushConst(beta);
    reqBuf.Push64BitPointer(C);     // device pointer
    reqBuf.Push(ldc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle, transa, transb, m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

cublasStatus_t cublasSgemmStridedBatched(cublasHandle_t handle, 
                                         cublasOperation_t transa, cublasOperation_t transb, 
                                         int m, int n, int k,
                                         const float* alpha, const float* A, int lda, long long int strideA,
                                         const float* B, int ldb, long long int strideB, const float* beta,
                                         float* C, int ldc, long long int strideC, int batchCount) {
    const char* func_name = "cublasSgemmStridedBatched";
    HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t, cublasOperation_t, cublasOperation_t, int, int, int, const float *,
                                       const float *, int, long long int, const float *, int, long long int, const float *,
                                       float *, int, long long int, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasSgemmStridedBatched"));

    // printf("alpha: %p, beta: %p\n", alpha, beta);

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cublasOperation_t) + sizeof(cublasOperation_t) + 3 * sizeof(int) + 2 * (sizeof(size_t)+sizeof(const float)) + 3 * sizeof(uint64_t) + 3 * sizeof(int) + 3 * sizeof(long long int) + sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_SGEMM_STRIDED_BATCHED);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(transa);
    reqBuf.Push(transb);
    reqBuf.Push(m);
    reqBuf.Push(n);
    reqBuf.Push(k);
    reqBuf.PushConst(alpha);
    reqBuf.Push64BitPointer(A);     // device pointer
    reqBuf.Push(lda);
    reqBuf.Push(strideA);
    reqBuf.Push64BitPointer(B);     // device pointer
    reqBuf.Push(ldb);
    reqBuf.Push(strideB);
    reqBuf.PushConst(beta);
    reqBuf.Push64BitPointer(C);     // device pointer
    reqBuf.Push(ldc);
    reqBuf.Push(strideC);
    reqBuf.Push(batchCount);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle, transa, transb, m, n, k, alpha, A, lda, strideA, B, ldb, strideB, beta, C, ldc, strideC, batchCount);
}

cublasStatus_t cublasDestroy_v2(cublasHandle_t handle) {
    const char* func_name = "cublasDestroy_v2";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasDestroy_v2"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_DESTROY_V2);
    reqBuf.Push64BitPointer(handle);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle);
}

//todo: to be validated
cublasStatus_t cublasSetStream_v2(cublasHandle_t handle, cudaStream_t streamId) {
    const char* func_name = "cublasSetStream_v2";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasSetStream_v2"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_SET_STREAM_V2);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(streamId);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle, streamId);
}

cublasStatus_t cublasSetWorkspace_v2(cublasHandle_t handle, 
                                     void *workspace, size_t workspaceSizeInBytes) {
    const char* func_name = "cublasSetWorkspace_v2";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t, void *, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasSetWorkspace_v2"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_SET_WORKSPACE_V2);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(workspace);
    reqBuf.Push(workspaceSizeInBytes);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle, workspace, workspaceSizeInBytes);
}

cublasStatus_t cublasSetMathMode(cublasHandle_t handle, cublasMath_t mode) {
    const char* func_name = "cublasSetMathMode";
	HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t, cublasMath_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasSetMathMode"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cublasMath_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_SET_MATH_MODE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUBLAS_STATUS_SUCCESS;
    
    //return func_entry(handle, mode);
}

cublasStatus_t cublasGetMathMode(cublasHandle_t handle, cublasMath_t* mode) {
    const char* func_name = "cublasGetMathMode";
    HookLog(func_name);
    using func_ptr = cublasStatus_t (*)(cublasHandle_t, cublasMath_t*);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cublasGetMathMode"));

    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUBLAS_GET_MATH_MODE);
    reqBuf.Push64BitPointer(handle);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(mode);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUBLAS_STATUS_SUCCESS;

    //return func_entry(handle, mode);
}


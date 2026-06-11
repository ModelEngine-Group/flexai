#include "../../include/hook/hook.h"

inline size_t getSizeOfAttributeType(cudnnBackendAttributeType_t attributeType) {
    size_t varSize = 0;
    switch (attributeType) {
    // ref: https://docs.nvidia.com/deeplearning/cudnn/api/cudnn-graph-library.html?highlight=cudnnbackendcreatedescriptor#id89
        case CUDNN_TYPE_HANDLE:
            varSize = sizeof(cudnnHandle_t);
            break;
        case CUDNN_TYPE_DATA_TYPE:
            varSize = sizeof(cudnnDataType_t);
            break;
        case CUDNN_TYPE_BOOLEAN:
            varSize = sizeof(bool);
            break;
        case CUDNN_TYPE_INT64:
            varSize = sizeof(int64_t);
            break;
        case CUDNN_TYPE_FLOAT:
            varSize = sizeof(float);
            break;
        case CUDNN_TYPE_DOUBLE:
            varSize = sizeof(double);
            break;
        case CUDNN_TYPE_VOID_PTR:
            varSize = sizeof(uint64_t); // maybe device pointer
            break;
        case CUDNN_TYPE_CONVOLUTION_MODE:
            varSize = sizeof(cudnnConvolutionMode_t);
            break;
        case CUDNN_TYPE_HEUR_MODE:
            varSize = sizeof(cudnnBackendHeurMode_t);
            break;
        case CUDNN_TYPE_KNOB_TYPE:
            varSize = sizeof(cudnnBackendKnobType_t);
            break;
        case CUDNN_TYPE_NAN_PROPOGATION:
            varSize = sizeof(cudnnNanPropagation_t);
            break;
        case CUDNN_TYPE_NUMERICAL_NOTE:
            varSize = sizeof(cudnnBackendNumericalNote_t);
            break;
        case CUDNN_TYPE_LAYOUT_TYPE:
            varSize = sizeof(cudnnBackendLayoutType_t);
            break;
        case CUDNN_TYPE_ATTRIB_NAME:
            varSize = sizeof(cudnnBackendAttributeName_t);
            break;
        case CUDNN_TYPE_POINTWISE_MODE:
            varSize = sizeof(cudnnPointwiseMode_t);
            break;
        case CUDNN_TYPE_BACKEND_DESCRIPTOR:
            varSize = sizeof(cudnnBackendDescriptor_t);
            break;
        case CUDNN_TYPE_GENSTATS_MODE:
            varSize = sizeof(cudnnGenStatsMode_t);
            break;
        case CUDNN_TYPE_BN_FINALIZE_STATS_MODE:
            varSize = sizeof(cudnnBnFinalizeStatsMode_t);
            break;
        case CUDNN_TYPE_REDUCTION_OPERATOR_TYPE:
            varSize = sizeof(cudnnReduceTensorOp_t);
            break;
        case CUDNN_TYPE_BEHAVIOR_NOTE:
            varSize = sizeof(cudnnBackendBehaviorNote_t);
            break;
        case CUDNN_TYPE_TENSOR_REORDERING_MODE:
            varSize = sizeof(cudnnBackendTensorReordering_t);
            break;
        case CUDNN_TYPE_RESAMPLE_MODE:
            varSize = sizeof(cudnnResampleMode_t);
            break;
        case CUDNN_TYPE_PADDING_MODE:
            varSize = sizeof(cudnnPaddingMode_t);
            break;
        case CUDNN_TYPE_INT32:
            varSize = sizeof(int32_t);
            break;
        case CUDNN_TYPE_CHAR:
            varSize = sizeof(char);
            break;
        case CUDNN_TYPE_SIGNAL_MODE:
            varSize = sizeof(cudnnSignalMode_t);
            break;
        case CUDNN_TYPE_FRACTION:
            varSize = sizeof(cudnnFraction_t);
            break;
        case CUDNN_TYPE_NORM_MODE:
            varSize = sizeof(cudnnBackendNormMode_t);
            break;
        case CUDNN_TYPE_NORM_FWD_PHASE:
            varSize = sizeof(cudnnBackendNormFwdPhase_t);
            break;
        default:
            varSize = sizeof(uint64_t); // Unknown type
            break;
        // case CUDNN_TYPE_RNG_DISTRIBUTION:
        //     varSize = sizeof(cudnnRngDistribution_t);
        //     break;
    }
    return varSize;
}

cudnnStatus_t cudnnCreate(cudnnHandle_t *handle) {
    const char* func_name = "cudnnCreate";
	HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnCreate"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CREATE);
    *handle = NULL;
    reqBuf.Push(handle);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(handle);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle);
}

cudnnStatus_t cudnnDestroy(cudnnHandle_t handle) {
    const char* func_name = "cudnnDestroy";
	HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnDestroy"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_DESTROY);
    reqBuf.Push64BitPointer(handle);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(handle);
}

cudnnStatus_t cudnnCreateTensorDescriptor(cudnnTensorDescriptor_t *tensorDesc) {
    const char* func_name = "cudnnCreateTensorDescriptor";
	HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorDescriptor_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnCreateTensorDescriptor"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CREATE_TENSOR_DESCRIPTOR);
    *tensorDesc = NULL;
    reqBuf.Push(tensorDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(tensorDesc);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    // clientEpObj->ReceiveResponse(sizeof(cudnnTensorDescriptor_t), tensorDesc);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(tensorDesc);
}

cudnnStatus_t cudnnDestroyTensorDescriptor(cudnnTensorDescriptor_t tensorDesc) {
    const char* func_name = "cudnnDestroyTensorDescriptor";
	HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnDestroyTensorDescriptor"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_DESTROY_TENSOR_DESCRIPTOR);
    reqBuf.Push64BitPointer(tensorDesc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(tensorDesc);
}

cudnnStatus_t cudnnGetTensorSizeInBytes(const cudnnTensorDescriptor_t tensorDesc,       
                                        size_t *size){
    const char* func_name = "cudnnGetTensorSizeInBytes";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(const cudnnTensorDescriptor_t, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetTensorSizeInBytes"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_TENSOR_SIZE_IN_BYTES);
    reqBuf.Push64BitPointer((cudnnTensorDescriptor_t)tensorDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(size);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(tensorDesc, size);
}

cudnnStatus_t cudnnSetTensor4dDescriptor(cudnnTensorDescriptor_t tensorDesc,
                                         cudnnTensorFormat_t format, cudnnDataType_t dataType, 
                                         int n, int c, int h, int w) {
    const char* func_name = "cudnnSetTensor4dDescriptor";
	HookLog(func_name);
    using func_ptr =
        cudnnStatus_t (*)(cudnnTensorDescriptor_t, cudnnTensorFormat_t, cudnnDataType_t, int, int, int, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetTensor4dDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnTensorFormat_t) + sizeof(cudnnDataType_t) + 4 * sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_TENSOR_4D_DESCRIPTOR);
    reqBuf.Push64BitPointer(tensorDesc);
    reqBuf.Push(format);
    reqBuf.Push(dataType);
    reqBuf.Push(n);
    reqBuf.Push(c);
    reqBuf.Push(h);
    reqBuf.Push(w);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(tensorDesc, format, dataType, n, c, h, w);
}

cudnnStatus_t cudnnSetTensorNdDescriptor(cudnnTensorDescriptor_t tensorDesc, cudnnDataType_t dataType, 
                                         int nbDims, const int dimA[], const int strideA[]) {
    const char* func_name = "cudnnSetTensorNdDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorDescriptor_t, cudnnDataType_t, int, const int[], const int[]);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetTensorNdDescriptor"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnDataType_t) + sizeof(int) + sizeof(size_t)+sizeof(const int)*nbDims + sizeof(size_t)+sizeof(const int)*nbDims);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_TENSOR_ND_DESCRIPTOR);
    reqBuf.Push64BitPointer(tensorDesc);
    reqBuf.Push(dataType);
    reqBuf.Push(nbDims);
    reqBuf.PushConst(dimA, nbDims);
    reqBuf.PushConst(strideA, nbDims);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(tensorDesc, dataType, nbDims, dimA, strideA);
}

cudnnStatus_t cudnnSetTensorNdDescriptorEx(cudnnTensorDescriptor_t tensorDesc, cudnnTensorFormat_t format,
                                           cudnnDataType_t dataType, int nbDims, const int dimA[]) {
    const char* func_name = "cudnnSetTensorNdDescriptorEx";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorDescriptor_t, cudnnTensorFormat_t, cudnnDataType_t, int, const int[]);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetTensorNdDescriptorEx"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnTensorFormat_t) + sizeof(cudnnDataType_t) + sizeof(int) + sizeof(size_t)+sizeof(int)*nbDims);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_TENSOR_ND_DESCRIPTOR_EX);
    reqBuf.Push64BitPointer(tensorDesc);
    reqBuf.Push(format);
    reqBuf.Push(dataType);
    reqBuf.Push(nbDims);
    reqBuf.PushConst(dimA, nbDims);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);

    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(tensorDesc, format, dataType, nbDims, dimA);
}

cudnnStatus_t cudnnCreateTensorTransformDescriptor(cudnnTensorTransformDescriptor_t *transformDesc) {
    const char* func_name = "cudnnCreateTensorTransformDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorTransformDescriptor_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnCreateTensorTransformDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CREATE_TENSOR_TRANSFORM_DESCRIPTOR);
    *transformDesc = NULL;
    reqBuf.Push(transformDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(transformDesc);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(transformDesc);
}

cudnnStatus_t cudnnSetTensorTransformDescriptor(
              cudnnTensorTransformDescriptor_t transformDesc, const uint32_t nbDims, 
              const cudnnTensorFormat_t destFormat, 
              const int32_t padBeforeA[], const int32_t padAfterA[], 
              const uint32_t foldA[], const cudnnFoldingDirection_t direction) {
    const char* func_name = "cudnnSetTensorTransformDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorTransformDescriptor_t, const uint32_t, const cudnnTensorFormat_t,
                                       const int32_t[], const int32_t[], const uint32_t[], const cudnnFoldingDirection_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetTensorTransformDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint32_t) + sizeof(cudnnTensorFormat_t) + sizeof(size_t)+sizeof(int32_t)*nbDims + sizeof(size_t)+sizeof(int32_t)*nbDims + sizeof(size_t)+sizeof(uint32_t)*nbDims + sizeof(cudnnFoldingDirection_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_TENSOR_TRANSFORM_DESCRIPTOR);
    reqBuf.Push64BitPointer(transformDesc);
    reqBuf.PushConst(nbDims);
    reqBuf.PushConst(destFormat);
    reqBuf.PushConst(padBeforeA, nbDims);
    reqBuf.PushConst(padAfterA, nbDims);
    reqBuf.PushConst(foldA, nbDims - 2); // spatial dimension (dimensions 2 and up)
    reqBuf.PushConst(direction);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(transformDesc, nbDims, destFormat, padBeforeA, padAfterA, foldA, direction);
}

cudnnStatus_t cudnnDestroyTensorTransformDescriptor(cudnnTensorTransformDescriptor_t transformDesc) {
    const char* func_name = "cudnnDestroyTensorTransformDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnTensorTransformDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnDestroyTensorTransformDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_DESTROY_TENSOR_TRANSFORM_DESCRIPTOR);
    reqBuf.Push64BitPointer(transformDesc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(transformDesc);
}

cudnnStatus_t cudnnInitTransformDest(
              const cudnnTensorTransformDescriptor_t transformDesc,
              const cudnnTensorDescriptor_t srcDesc, 
              cudnnTensorDescriptor_t destDesc, size_t *destSizeInBytes) {
    const char* func_name = "cudnnInitTransformDest";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(const cudnnTensorTransformDescriptor_t, const cudnnTensorDescriptor_t,
                                       cudnnTensorDescriptor_t, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnInitTransformDest"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_INIT_TRANSFORM_DEST);
    reqBuf.Push64BitPointer(transformDesc);
    reqBuf.Push64BitPointer(srcDesc);
    reqBuf.Push64BitPointer(destDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(destSizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(transformDesc, srcDesc, destDesc, destSizeInBytes);
}

cudnnStatus_t cudnnTransformTensorEx(
              cudnnHandle_t handle,
              const cudnnTensorTransformDescriptor_t transDesc,
              const void *alpha, 
              const cudnnTensorDescriptor_t srcDesc, const void *srcData,
              const void *beta,const cudnnTensorDescriptor_t destDesc,
              void *destData) {
    const char* func_name = "cudnnTransformTensorEx";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const cudnnTensorTransformDescriptor_t, const void *,
                                       const cudnnTensorDescriptor_t, const void *, const void *,
                                       const cudnnTensorDescriptor_t, void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnTransformTensorEx"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*2 + sizeof(uint64_t)*2);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_TRANSFORM_TENSOR_EX);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(transDesc);
    reqBuf.PushConst((const float*)alpha);
    reqBuf.Push64BitPointer(srcDesc);
    reqBuf.Push64BitPointer(srcData); // Data pointer to GPU memory, not to host memory in official documetation
    reqBuf.PushConst((const float*)beta);
    reqBuf.Push64BitPointer(destDesc);
    reqBuf.Push64BitPointer(destData); // Data pointer to GPU memory, not to host memory in official documetation
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, transDesc, alpha, srcDesc, srcData, beta, destDesc, destData);
}

cudnnStatus_t cudnnTransformFilter(
              cudnnHandle_t handle,
              const cudnnTensorTransformDescriptor_t transDesc,
              const void *alpha,
              const cudnnFilterDescriptor_t srcDesc, const void *srcData,
              const void *beta,
              const cudnnFilterDescriptor_t destDesc, void *destData) {
    const char* func_name = "cudnnTransformFilter";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const cudnnTensorTransformDescriptor_t, const void *,
                                       const cudnnFilterDescriptor_t, const void *, const void *,
                                       const cudnnFilterDescriptor_t, void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnTransformFilter"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*2 + sizeof(uint64_t)*2);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_TRANSFORM_FILTER);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(transDesc);
    reqBuf.PushConst((const float*)alpha);
    reqBuf.Push64BitPointer(srcDesc);
    reqBuf.Push64BitPointer(srcData); // Data pointer to GPU memory, not to host memory in official documetation
    reqBuf.PushConst((const float*)beta);
    reqBuf.Push64BitPointer(destDesc);
    reqBuf.Push64BitPointer(destData); // Data pointer to GPU memory, not to host memory in official documetation
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, transDesc, alpha, srcDesc, srcData, beta, destDesc, destData);
}

cudnnStatus_t cudnnCreateFilterDescriptor(cudnnFilterDescriptor_t *filterDesc) {
    const char* func_name = "cudnnCreateFilterDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnFilterDescriptor_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnCreateFilterDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CREATE_FILTER_DESCRIPTOR);
    *filterDesc = NULL;
    reqBuf.Push(filterDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(filterDesc);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(filterDesc);
}

cudnnStatus_t cudnnSetFilterNdDescriptor(cudnnFilterDescriptor_t filterDesc,
                                         cudnnDataType_t dataType, cudnnTensorFormat_t format, 
                                         int nbDims, const int filterDimA[]) {
    const char* func_name = "cudnnSetFilterNdDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnFilterDescriptor_t, cudnnDataType_t, cudnnTensorFormat_t, int, const int[]);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetFilterNdDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnDataType_t) + sizeof(cudnnTensorFormat_t) + sizeof(int) + sizeof(size_t)+sizeof(int)*nbDims);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_FILTER_ND_DESCRIPTOR);
    reqBuf.Push64BitPointer(filterDesc);
    reqBuf.Push(dataType);
    reqBuf.Push(format);
    reqBuf.Push(nbDims);
    reqBuf.PushConst(filterDimA, nbDims);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(filterDesc, dataType, format, nbDims, filterDimA);
}

cudnnStatus_t cudnnDestroyFilterDescriptor(cudnnFilterDescriptor_t filterDesc) {
    const char* func_name = "cudnnDestroyFilterDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnFilterDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnDestroyFilterDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_DESTROY_FILTER_DESCRIPTOR);
    reqBuf.Push64BitPointer(filterDesc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(filterDesc);
}

cudnnStatus_t cudnnGetFilterSizeInBytes(const cudnnFilterDescriptor_t filterDesc, size_t *size) {
    const char* func_name = "cudnnGetFilterSizeInBytes";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(const cudnnFilterDescriptor_t, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetFilterSizeInBytes"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_FILTER_SIZE_IN_BYTES);
    reqBuf.Push64BitPointer(filterDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(size);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(filterDesc, size);
}

cudnnStatus_t cudnnGetFoldedConvBackwardDataDescriptors(
              const cudnnHandle_t handle, const cudnnFilterDescriptor_t filterDesc,
              const cudnnTensorDescriptor_t diffDesc, const cudnnConvolutionDescriptor_t convDesc,
              const cudnnTensorDescriptor_t gradDesc, const cudnnTensorFormat_t transformFormat,
              cudnnFilterDescriptor_t foldedFilterDesc, cudnnTensorDescriptor_t paddedDiffDesc,
              cudnnConvolutionDescriptor_t foldedConvDesc, cudnnTensorDescriptor_t foldedGradDesc,
              cudnnTensorTransformDescriptor_t filterFoldTransDesc, cudnnTensorTransformDescriptor_t diffPadTransDesc,
              cudnnTensorTransformDescriptor_t gradFoldTransDesc, cudnnTensorTransformDescriptor_t gradUnfoldTransDesc) {
    const char* func_name = "cudnnGetFoldedConvBackwardDataDescriptors";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(const cudnnHandle_t, const cudnnFilterDescriptor_t, const cudnnTensorDescriptor_t,
                                       const cudnnConvolutionDescriptor_t, const cudnnTensorDescriptor_t, const cudnnTensorFormat_t,
                                       cudnnFilterDescriptor_t, cudnnTensorDescriptor_t, cudnnConvolutionDescriptor_t, cudnnTensorDescriptor_t,
                                       cudnnTensorTransformDescriptor_t, cudnnTensorTransformDescriptor_t, cudnnTensorTransformDescriptor_t, cudnnTensorTransformDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetFoldedConvBackwardDataDescriptors"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t)*4 + sizeof(cudnnTensorFormat_t) + sizeof(uint64_t)*8);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_FOLDED_CONV_BACKWARD_DATA_DESCRIPTORS);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(filterDesc);
    reqBuf.Push64BitPointer(diffDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(gradDesc);
    reqBuf.PushConst(transformFormat);
    reqBuf.Push64BitPointer(foldedFilterDesc);
    reqBuf.Push64BitPointer(paddedDiffDesc);
    reqBuf.Push64BitPointer(foldedConvDesc);
    reqBuf.Push64BitPointer(foldedGradDesc);
    reqBuf.Push64BitPointer(filterFoldTransDesc);
    reqBuf.Push64BitPointer(diffPadTransDesc);
    reqBuf.Push64BitPointer(gradFoldTransDesc);
    reqBuf.Push64BitPointer(gradUnfoldTransDesc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, filterDesc, diffDesc, convDesc, gradDesc, transformFormat, foldedFilterDesc, paddedDiffDesc, foldedConvDesc, foldedGradDesc, filterFoldTransDesc, diffPadTransDesc, gradFoldTransDesc, gradUnfoldTransDesc);
}

cudnnStatus_t cudnnSetStream(cudnnHandle_t handle, cudaStream_t streamId) {
    const char* func_name = "cudnnSetStream";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, cudaStream_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetStream"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_STREAM);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(streamId);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, streamId);
}

cudnnStatus_t cudnnBatchNormalizationBackwardEx(
              cudnnHandle_t handle, cudnnBatchNormMode_t mode, cudnnBatchNormOps_t bnOps,
              const void *alphaDataDiff,  const void *betaDataDiff, 
              const void *alphaParamDiff, const void *betaParamDiff,
              const cudnnTensorDescriptor_t xDesc,  const void *xData,
              const cudnnTensorDescriptor_t yDesc,  const void *yData,
              const cudnnTensorDescriptor_t dyDesc, const void *dyData, 
              const cudnnTensorDescriptor_t dzDesc, void *dzData,
              const cudnnTensorDescriptor_t dxDesc, void *dxData,
              const cudnnTensorDescriptor_t dBnScaleBiasDesc,
              const void *bnScaleData, const void *bnBiasData,
              void *dBnScaleData, void *dBnBiasData, double epsilon,
              const void *savedMean, const void *savedInvVariance, 
              cudnnActivationDescriptor_t activationDesc, 
              void *workSpace, size_t workSpaceSizeInBytes, 
              void *reserveSpace, size_t reserveSpaceSizeInBytes) {
    const char* func_name = "cudnnBatchNormalizationBackwardEx";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(
        cudnnHandle_t, cudnnBatchNormMode_t, cudnnBatchNormOps_t, const void *, const void *, const void *,
        const void *, const cudnnTensorDescriptor_t, const void *, const cudnnTensorDescriptor_t, const void *,
        const cudnnTensorDescriptor_t, const void *, const cudnnTensorDescriptor_t, void *,
        const cudnnTensorDescriptor_t, void *, const cudnnTensorDescriptor_t, const void *, const void *, void *,
        void *, double, const void *, const void *, cudnnActivationDescriptor_t, void *, size_t, void *, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBatchNormalizationBackwardEx"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBatchNormMode_t) + sizeof(cudnnBatchNormOps_t) + (sizeof(size_t)+sizeof(const float))*4 + sizeof(uint64_t)*15 + sizeof(double) + sizeof(uint64_t)*5 + sizeof(uint64_t)*2);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BATCH_NORMALIZATION_BACKWARD_EX);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    reqBuf.Push(bnOps);
    reqBuf.PushConst((const float*)alphaDataDiff); // Pointers to scaling factors (in host memory)
    reqBuf.PushConst((const float*)betaDataDiff);
    reqBuf.PushConst((const float*)alphaParamDiff);
    reqBuf.PushConst((const float*)betaParamDiff);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(xData); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push64BitPointer(yData); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(dyData); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(dzDesc);
    reqBuf.Push64BitPointer(dzData); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(dxDesc);
    reqBuf.Push64BitPointer(dxData); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(dBnScaleBiasDesc);
    reqBuf.Push64BitPointer(bnScaleData); // located in GPU memory
    reqBuf.Push64BitPointer(bnBiasData);
    reqBuf.Push64BitPointer(dBnScaleData);
    reqBuf.Push64BitPointer(dBnBiasData);
    reqBuf.Push(epsilon);
    reqBuf.Push64BitPointer(savedMean);
    reqBuf.Push64BitPointer(savedInvVariance);
    reqBuf.Push64BitPointer(activationDesc);
    reqBuf.Push64BitPointer(workSpace);
    reqBuf.Push(workSpaceSizeInBytes);
    reqBuf.Push64BitPointer(reserveSpace);
    reqBuf.Push(reserveSpaceSizeInBytes);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, mode, bnOps, alphaDataDiff, betaDataDiff, alphaParamDiff, betaParamDiff, xDesc, xData, yDesc, yData, dyDesc, dyData, dzDesc, dzData, dxDesc, dxData, dBnScaleBiasDesc, bnScaleData, bnBiasData, dBnScaleData, dBnBiasData, epsilon, savedMean, savedInvVariance, activationDesc, workSpace, workSpaceSizeInBytes, reserveSpace, reserveSpaceSizeInBytes);
}

cudnnStatus_t cudnnBatchNormalizationForwardTrainingEx(
              cudnnHandle_t handle, cudnnBatchNormMode_t mode, cudnnBatchNormOps_t bnOps, 
              const void *alpha, const void *beta,
              const cudnnTensorDescriptor_t xDesc, const void *xData,
              const cudnnTensorDescriptor_t zDesc, const void *zData,
              const cudnnTensorDescriptor_t yDesc, void *yData, 
              const cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc,
              const void *bnScaleData, const void *bnBiasData, double exponentialAverageFactor, 
              void *resultRunningMean, void *resultRunningVariance, double epsilon, 
              void *resultSaveMean, void *resultSaveInvVariance,
              const cudnnActivationDescriptor_t activationDesc, 
              void *workspace,    size_t workSpaceSizeInBytes, 
              void *reserveSpace, size_t reserveSpaceSizeInBytes) {
    const char* func_name = "cudnnBatchNormalizationForwardTrainingEx";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(
        cudnnHandle_t, cudnnBatchNormMode_t, cudnnBatchNormOps_t, const void *, const void *,
        const cudnnTensorDescriptor_t, const void *, const cudnnTensorDescriptor_t, const void *,
        const cudnnTensorDescriptor_t, void *, const cudnnTensorDescriptor_t, const void *, const void *, double,
        void *, void *, double, void *, void *, const cudnnActivationDescriptor_t, void *, size_t, void *, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBatchNormalizationForwardTrainingEx"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBatchNormMode_t) + sizeof(cudnnBatchNormOps_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*9 + sizeof(double) + sizeof(uint64_t)*2 + sizeof(double) + sizeof(uint64_t)*3 + sizeof(uint64_t)*2 + sizeof(size_t)*2);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BATCH_NORMALIZATION_FORWARD_TRAINING_EX);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    reqBuf.Push(bnOps);
    reqBuf.PushConst((const float*)alpha); // Pointers to scaling factors (in host memory)
    reqBuf.PushConst((const float*)beta);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(xData); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(zDesc);
    reqBuf.Push64BitPointer(zData);
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push64BitPointer(yData);
    reqBuf.Push64BitPointer(bnScaleBiasMeanVarDesc);
    reqBuf.Push64BitPointer(bnScaleData); // located in GPU memory
    reqBuf.Push64BitPointer(bnBiasData);
    reqBuf.Push(exponentialAverageFactor);
    reqBuf.Push64BitPointer(resultRunningMean);
    reqBuf.Push64BitPointer(resultRunningVariance);
    reqBuf.Push(epsilon);
    reqBuf.Push64BitPointer(resultSaveMean);
    reqBuf.Push64BitPointer(resultSaveInvVariance);
    reqBuf.Push64BitPointer(activationDesc);
    reqBuf.Push64BitPointer(workspace);
    reqBuf.Push(workSpaceSizeInBytes);
    reqBuf.Push64BitPointer(reserveSpace);
    reqBuf.Push(reserveSpaceSizeInBytes);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(handle, mode, bnOps, alpha, beta, xDesc, xData, zDesc, zData, yDesc, yData, bnScaleBiasMeanVarDesc, bnScaleData, bnBiasData, exponentialAverageFactor, resultRunningMean, resultRunningVariance, epsilon, resultSaveMean, resultSaveInvVariance, activationDesc, workspace, workSpaceSizeInBytes, reserveSpace, reserveSpaceSizeInBytes);
}

cudnnStatus_t cudnnBatchNormalizationForwardInference( 
              cudnnHandle_t handle, cudnnBatchNormMode_t mode, 
              const void *alpha, const void *beta,
              const cudnnTensorDescriptor_t xDesc, const void *x, 
              const cudnnTensorDescriptor_t yDesc, void *y,
              const cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc, 
              const void *bnScale, const void *bnBias,
              const void *estimatedMean, const void *estimatedVariance, double epsilon) {
    const char* func_name = "cudnnBatchNormalizationForwardInference";
	HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, cudnnBatchNormMode_t, const void *, const void *,
                                       const cudnnTensorDescriptor_t, const void *, const cudnnTensorDescriptor_t,
                                       void *, const cudnnTensorDescriptor_t, const void *, const void *, const void *,
                                       const void *, double);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBatchNormalizationForwardInference"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBatchNormMode_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*9 + sizeof(double)); 
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BATCH_NORMALIZATION_FORWARD_INFERENCE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    reqBuf.PushConst((const float*)alpha); // Pointers to scaling factors (in host memory)
    reqBuf.PushConst((const float*)beta); 
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(x); // Data pointer to GPU memory
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push64BitPointer(y);
    reqBuf.Push64BitPointer(bnScaleBiasMeanVarDesc);
    reqBuf.Push64BitPointer(bnScale); // located in GPU memory
    reqBuf.Push64BitPointer(bnBias);
    reqBuf.Push64BitPointer(estimatedMean);
    reqBuf.Push64BitPointer(estimatedVariance);
    reqBuf.Push(epsilon);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, mode, alpha, beta, xDesc, x, yDesc, y, bnScaleBiasMeanVarDesc, bnScale, bnBias, estimatedMean, estimatedVariance, epsilon);
}

cudnnStatus_t cudnnBackendCreateDescriptor(cudnnBackendDescriptorType_t descriptorType, cudnnBackendDescriptor_t *descriptor) {
    const char* func_name = "cudnnBackendCreateDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnBackendDescriptorType_t, cudnnBackendDescriptor_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBackendCreateDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(cudnnBackendDescriptorType_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BACKEND_CREATE_DESCRIPTOR);
    reqBuf.Push(descriptorType);
    *descriptor = NULL;
    reqBuf.Push(descriptor);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(descriptor);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(descriptorType, descriptor);
}

cudnnStatus_t cudnnBackendDestroyDescriptor(cudnnBackendDescriptor_t descriptor) {
    const char* func_name = "cudnnBackendDestroyDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnBackendDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBackendDestroyDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BACKEND_DESTROY_DESCRIPTOR);
    reqBuf.Push64BitPointer(descriptor);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(descriptor);
}

cudnnStatus_t cudnnBackendSetAttribute(
              cudnnBackendDescriptor_t descriptor, cudnnBackendAttributeName_t attributeName,
              cudnnBackendAttributeType_t attributeType, 
              int64_t elementCount, const void *arrayOfElements) { //! arrayOfElements is const void* type, not void* type declared in the official document
    const char* func_name = "cudnnBackendSetAttribute";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnBackendDescriptor_t, cudnnBackendAttributeName_t, cudnnBackendAttributeType_t, int64_t, const void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBackendSetAttribute"));

    size_t varSize = getSizeOfAttributeType(attributeType);
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBackendAttributeName_t) + sizeof(cudnnBackendAttributeType_t) + sizeof(int64_t) + sizeof(size_t)+varSize*elementCount);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BACKEND_SET_ATTRIBUTE);
    reqBuf.Push64BitPointer(descriptor);
    reqBuf.Push(attributeName);
    reqBuf.Push(attributeType);
    reqBuf.Push(elementCount);
    reqBuf.PushConst((const uint8_t*)arrayOfElements, varSize * elementCount); 

    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(descriptor, attributeName, attributeType, elementCount, arrayOfElements);
}

cudnnStatus_t cudnnBackendGetAttribute(
              cudnnBackendDescriptor_t descriptor, cudnnBackendAttributeName_t attributeName,
              cudnnBackendAttributeType_t attributeType, int64_t requestedElementCount,
              int64_t *elementCount, void *arrayOfElements) {
    const char* func_name = "cudnnBackendGetAttribute";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnBackendDescriptor_t, cudnnBackendAttributeName_t, cudnnBackendAttributeType_t, int64_t, int64_t *, void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBackendGetAttribute"));

    size_t varSize = getSizeOfAttributeType(attributeType);
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBackendAttributeName_t) + sizeof(cudnnBackendAttributeType_t) + sizeof(int64_t) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BACKEND_GET_ATTRIBUTE);
    reqBuf.Push64BitPointer(descriptor);
    reqBuf.Push(attributeName);
    reqBuf.Push(attributeType);
    reqBuf.Push(requestedElementCount);
    reqBuf.Push(varSize); // notify the server the size of each element

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(elementCount);
    resBuf.Push((uint8_t*)arrayOfElements, varSize * requestedElementCount);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;

    // return func_entry(descriptor, attributeName, attributeType, requestedElementCount, elementCount, arrayOfElements);
}

cudnnStatus_t cudnnBackendExecute(cudnnHandle_t handle, cudnnBackendDescriptor_t executionPlan, 
                                  cudnnBackendDescriptor_t varianPack) {
    const char* func_name = "cudnnBackendExecute";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, cudnnBackendDescriptor_t, cudnnBackendDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBackendExecute"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BACKEND_EXECUTE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(executionPlan);
    reqBuf.Push64BitPointer(varianPack);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, executionPlan, varianPack);
}

cudnnStatus_t cudnnBackendFinalize(cudnnBackendDescriptor_t descriptor) {
    const char* func_name = "cudnnBackendFinalize";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnBackendDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnBackendFinalize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_BACKEND_FINALIZE);
    reqBuf.Push64BitPointer(descriptor);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(descriptor);
}

cudnnStatus_t cudnnGetBatchNormalizationBackwardExWorkspaceSize(
              cudnnHandle_t handle, cudnnBatchNormMode_t mode, 
              cudnnBatchNormOps_t bnOps, const cudnnTensorDescriptor_t xDesc, 
              const cudnnTensorDescriptor_t yDesc, const cudnnTensorDescriptor_t dyDesc, 
              const cudnnTensorDescriptor_t dzDesc,const cudnnTensorDescriptor_t dxDesc, 
              const cudnnTensorDescriptor_t dBnScaleBiasDesc,
    const cudnnActivationDescriptor_t activationDesc, size_t *sizeInBytes) {
    const char* func_name = "cudnnGetBatchNormalizationBackwardExWorkspaceSize";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(
        cudnnHandle_t, cudnnBatchNormMode_t, cudnnBatchNormOps_t, const cudnnTensorDescriptor_t,
        const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t,
        const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t, const cudnnActivationDescriptor_t, size_t *);
    auto func_entry =
        reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetBatchNormalizationBackwardExWorkspaceSize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBatchNormMode_t) + sizeof(cudnnBatchNormOps_t) + sizeof(uint64_t) * 7);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_BATCH_NORMALIZATION_BACKWARD_EX_WORKSPACE_SIZE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    reqBuf.Push(bnOps);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(dzDesc);
    reqBuf.Push64BitPointer(dxDesc);
    reqBuf.Push64BitPointer(dBnScaleBiasDesc);
    reqBuf.Push64BitPointer(activationDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(sizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(handle, mode, bnOps, xDesc, yDesc, dyDesc, dzDesc, dxDesc, dBnScaleBiasDesc, activationDesc, sizeInBytes);
}

cudnnStatus_t cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize(
              cudnnHandle_t handle, cudnnBatchNormMode_t mode, 
              cudnnBatchNormOps_t bnOps, const cudnnTensorDescriptor_t xDesc, 
              const cudnnTensorDescriptor_t zDesc, const cudnnTensorDescriptor_t yDesc,
              const cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc, 
              const cudnnActivationDescriptor_t activationDesc, size_t *sizeInBytes) {
    const char* func_name = "cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize";
    HookLog(func_name);
    using func_ptr =
        cudnnStatus_t (*)(cudnnHandle_t, cudnnBatchNormMode_t, cudnnBatchNormOps_t, const cudnnTensorDescriptor_t,
                          const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t,
                          const cudnnActivationDescriptor_t, size_t *);
    auto func_entry =
        reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBatchNormMode_t) + sizeof(cudnnBatchNormOps_t) + sizeof(uint64_t) * 5);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_BATCH_NORMALIZATION_FORWARD_TRAINING_EX_WORKSPACE_SIZE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    reqBuf.Push(bnOps);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(zDesc);
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push64BitPointer(bnScaleBiasMeanVarDesc);
    reqBuf.Push64BitPointer(activationDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(sizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(handle, mode, bnOps, xDesc, zDesc, yDesc, bnScaleBiasMeanVarDesc, activationDesc, sizeInBytes);
}

cudnnStatus_t cudnnGetBatchNormalizationTrainingExReserveSpaceSize(
              cudnnHandle_t handle, cudnnBatchNormMode_t mode, cudnnBatchNormOps_t bnOps,
              const cudnnActivationDescriptor_t activationDesc, 
              const cudnnTensorDescriptor_t xDesc, size_t *sizeInBytes) {
    const char* func_name = "cudnnGetBatchNormalizationTrainingExReserveSpaceSize";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, cudnnBatchNormMode_t, cudnnBatchNormOps_t,
                                       const cudnnActivationDescriptor_t, const cudnnTensorDescriptor_t, size_t *);
    auto func_entry =
        reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetBatchNormalizationTrainingExReserveSpaceSize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnBatchNormMode_t) + sizeof(cudnnBatchNormOps_t) + sizeof(uint64_t) * 2);
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_BATCH_NORMALIZATION_TRAINING_EX_RESERVE_SPACE_SIZE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push(mode);
    reqBuf.Push(bnOps);
    reqBuf.Push64BitPointer(activationDesc);
    reqBuf.Push64BitPointer(xDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(sizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;
    
    //return func_entry(handle, mode, bnOps, activationDesc, xDesc, sizeInBytes);
}

cudnnStatus_t cudnnCreateConvolutionDescriptor(cudnnConvolutionDescriptor_t *convDesc) {
    const char* func_name = "cudnnCreateConvolutionDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnConvolutionDescriptor_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnCreateConvolutionDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CREATE_CONVOLUTION_DESCRIPTOR);
    *convDesc = NULL;
    reqBuf.Push(convDesc);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(convDesc);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(convDesc);
}

cudnnStatus_t cudnnDestroyConvolutionDescriptor(cudnnConvolutionDescriptor_t convDesc) {
    const char* func_name = "cudnnDestroyConvolutionDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnConvolutionDescriptor_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnDestroyConvolutionDescriptor"));
    
    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_DESTROY_CONVOLUTION_DESCRIPTOR);
    reqBuf.Push64BitPointer(convDesc);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(convDesc);
}

cudnnStatus_t cudnnSetConvolutionGroupCount(cudnnConvolutionDescriptor_t convDesc,
                                            int groupCount) {
    const char* func_name = "cudnnSetConvolutionGroupCount";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnConvolutionDescriptor_t, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetConvolutionGroupCount"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_CONVOLUTION_GROUP_COUNT);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(groupCount);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(convDesc, groupCount);
}

cudnnStatus_t cudnnSetConvolutionMathType(cudnnConvolutionDescriptor_t convDesc,
                                          cudnnMathType_t mathType) {
    const char* func_name = "cudnnSetConvolutionMathType";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnConvolutionDescriptor_t, cudnnMathType_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetConvolutionMathType"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnMathType_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_CONVOLUTION_MATH_TYPE);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(mathType);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(convDesc, mathType);
}

cudnnStatus_t cudnnSetConvolutionNdDescriptor(cudnnConvolutionDescriptor_t convDesc,
                                              int arrayLength, const int padA[],
                                              const int filterStrideA[], const int dilationA[],
                                              cudnnConvolutionMode_t mode, cudnnDataType_t dataType) {
    const char* func_name = "cudnnSetConvolutionNdDescriptor";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnConvolutionDescriptor_t, int, const int[], const int[], const int[], cudnnConvolutionMode_t, cudnnDataType_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetConvolutionNdDescriptor"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(int) + (sizeof(size_t)+sizeof(int)*arrayLength)* 3 + sizeof(cudnnConvolutionMode_t) + sizeof(cudnnDataType_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_CONVOLUTION_ND_DESCRIPTOR);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(arrayLength);
    reqBuf.PushConst(padA, arrayLength);
    reqBuf.PushConst(filterStrideA, arrayLength);
    reqBuf.PushConst(dilationA, arrayLength);
    reqBuf.Push(mode);
    reqBuf.Push(dataType);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(convDesc, arrayLength, padA, filterStrideA, dilationA, mode, dataType);
}

cudnnStatus_t cudnnSetConvolutionReorderType(cudnnConvolutionDescriptor_t convDesc,
                                             cudnnReorderType_t reorderType) {
    const char* func_name = "cudnnSetConvolutionReorderType";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnConvolutionDescriptor_t, cudnnReorderType_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnSetConvolutionReorderType"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(cudnnReorderType_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_SET_CONVOLUTION_REORDER_TYPE);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(reorderType);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(convDesc, reorderType);
}

// cudnnStatus_t cudnnReorderFilterAndBias(
//               cudnnHandle_t handle,
//               const cudnnFilterDescriptor_t filterDesc, cudnnReorderType_t reorderType,
//               const void *filterData, void *reorderedFilterData,
//               int reorderBias, const void *biasData, void *reorderedBiasData) {
//     const char* func_name = "cudnnReorderFilterAndBias";
//     HookLog(func_name);
//     using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const cudnnFilterDescriptor_t, cudnnReorderType_t, const void *, void *, int, const void *, void *);
//     auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnReorderFilterAndBias"));

//     RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) + sizeof(cudnnReorderType_t) + sizeof(uint64_t) * 2 + sizeof(int) + sizeof(uint64_t) * 2);
// }

cudnnStatus_t cudnnGetConvolutionForwardAlgorithm_v7(
              cudnnHandle_t handle, const cudnnTensorDescriptor_t xDesc, 
              const cudnnFilterDescriptor_t wDesc,
              const cudnnConvolutionDescriptor_t convDesc, 
              const cudnnTensorDescriptor_t yDesc,
              const int requestedAlgoCount, int *returnedAlgoCount, 
              cudnnConvolutionFwdAlgoPerf_t *perfResults) {
    const char* func_name = "cudnnGetConvolutionForwardAlgorithm_v7";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)( cudnnHandle_t, const cudnnTensorDescriptor_t, const cudnnFilterDescriptor_t, const cudnnConvolutionDescriptor_t, const cudnnTensorDescriptor_t, const int, int *, cudnnConvolutionFwdAlgoPerf_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetConvolutionForwardAlgorithm_v7"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) * 4 + sizeof(const int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_CONVOLUTION_FORWARD_ALGORITHM_V7);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(wDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.PushConst(requestedAlgoCount);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(returnedAlgoCount);
    resBuf.Push(perfResults, requestedAlgoCount);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, xDesc, wDesc, convDesc, yDesc, requestedAlgoCount, returnedAlgoCount, perfResults);
}

cudnnStatus_t cudnnGetConvolutionBackwardFilterAlgorithm_v7(
              cudnnHandle_t handle, const cudnnTensorDescriptor_t xDesc,
              const cudnnTensorDescriptor_t dyDesc,
              const cudnnConvolutionDescriptor_t convDesc,
              const cudnnFilterDescriptor_t dwDesc,
              const int requestedAlgoCount, int *returnedAlgoCount,
              cudnnConvolutionBwdFilterAlgoPerf_t *perfResults) {
    const char* func_name = "cudnnGetConvolutionBackwardFilterAlgorithm_v7";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)( cudnnHandle_t, const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t, const cudnnConvolutionDescriptor_t, const cudnnFilterDescriptor_t, const int, int *, cudnnConvolutionBwdFilterAlgoPerf_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetConvolutionBackwardFilterAlgorithm_v7"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) * 4 + sizeof(const int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_ALGORITHM_V7);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(dwDesc);
    reqBuf.PushConst(requestedAlgoCount);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(returnedAlgoCount);
    resBuf.Push(perfResults, requestedAlgoCount);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, xDesc, dyDesc, convDesc, dwDesc, requestedAlgoCount, returnedAlgoCount, perfResults);
}

cudnnStatus_t cudnnGetConvolutionBackwardDataAlgorithm_v7(
              cudnnHandle_t handle, const cudnnFilterDescriptor_t wDesc,
              const cudnnTensorDescriptor_t dyDesc,
              const cudnnConvolutionDescriptor_t convDesc,
              const cudnnTensorDescriptor_t dxDesc,
              const int requestedAlgoCount, int *returnedAlgoCount,
              cudnnConvolutionBwdDataAlgoPerf_t *perfResults) {
    const char* func_name = "cudnnGetConvolutionBackwardDataAlgorithm_v7";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)( cudnnHandle_t, const cudnnFilterDescriptor_t, const cudnnTensorDescriptor_t, const cudnnConvolutionDescriptor_t, const cudnnTensorDescriptor_t, const int, int *, cudnnConvolutionBwdDataAlgoPerf_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetConvolutionBackwardDataAlgorithm_v7"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) * 4 + sizeof(const int));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_DATA_ALGORITHM_V7);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(wDesc);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(dxDesc);
    reqBuf.PushConst(requestedAlgoCount);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(returnedAlgoCount);
    resBuf.Push(perfResults, requestedAlgoCount);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, wDesc, dyDesc, convDesc, dxDesc, requestedAlgoCount, returnedAlgoCount, perfResults);
}

cudnnStatus_t cudnnGetConvolutionForwardWorkspaceSize(
              cudnnHandle_t handle, const cudnnTensorDescriptor_t xDesc,
              const cudnnFilterDescriptor_t wDesc,
              const cudnnConvolutionDescriptor_t convDesc,
              const cudnnTensorDescriptor_t yDesc,
              cudnnConvolutionFwdAlgo_t algo, size_t *sizeInBytes) {
    const char* func_name = "cudnnGetConvolutionForwardWorkspaceSize";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const cudnnTensorDescriptor_t, const cudnnFilterDescriptor_t, const cudnnConvolutionDescriptor_t, const cudnnTensorDescriptor_t, cudnnConvolutionFwdAlgo_t, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetConvolutionForwardWorkspaceSize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) * 4 + sizeof(cudnnConvolutionFwdAlgo_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_CONVOLUTION_FORWARD_WORKSPACE_SIZE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(wDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push(algo);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(sizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, xDesc, wDesc, convDesc, yDesc, algo, sizeInBytes);
}

cudnnStatus_t cudnnConvolutionForward(
              cudnnHandle_t handle, const void *alpha,
              const cudnnTensorDescriptor_t xDesc, const void *x,
              const cudnnFilterDescriptor_t wDesc, const void *w,
              const cudnnConvolutionDescriptor_t  convDesc,
              cudnnConvolutionFwdAlgo_t           algo,
              void *workSpace, size_t workSpaceSizeInBytes,
              const void *beta, 
              const cudnnTensorDescriptor_t yDesc, void *y){
    const char* func_name = "cudnnConvolutionForward";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const void *, const cudnnTensorDescriptor_t, const void *, const cudnnFilterDescriptor_t, const void *, const cudnnConvolutionDescriptor_t, cudnnConvolutionFwdAlgo_t, void *, size_t, const void *, const cudnnTensorDescriptor_t, void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnConvolutionForward"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*6 + sizeof(uint64_t) + sizeof(cudnnConvolutionFwdAlgo_t) + sizeof(uint64_t) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CONVOLUTION_FORWARD);
    reqBuf.Push64BitPointer(handle);
    reqBuf.PushConst((const float*)alpha);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(x);
    reqBuf.Push64BitPointer(wDesc);
    reqBuf.Push64BitPointer(w);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(algo);
    reqBuf.Push64BitPointer(workSpace);
    reqBuf.Push(workSpaceSizeInBytes);
    reqBuf.PushConst((const float*)beta);
    reqBuf.Push64BitPointer(yDesc);
    reqBuf.Push64BitPointer(y);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, alpha, xDesc, x, wDesc, w, convDesc, algo, workSpace, workSpaceSizeInBytes, beta, yDesc, y);
}

cudnnStatus_t cudnnGetConvolutionBackwardDataWorkspaceSize(
              cudnnHandle_t handle, const cudnnFilterDescriptor_t wDesc,
              const cudnnTensorDescriptor_t dyDesc,
              const cudnnConvolutionDescriptor_t convDesc,
              const cudnnTensorDescriptor_t dxDesc,
              cudnnConvolutionBwdDataAlgo_t algo, size_t *sizeInBytes) {
    const char* func_name = "cudnnGetConvolutionBackwardDataWorkspaceSize";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const cudnnFilterDescriptor_t, const cudnnTensorDescriptor_t, const cudnnConvolutionDescriptor_t, const cudnnTensorDescriptor_t, cudnnConvolutionBwdDataAlgo_t, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetConvolutionBackwardDataWorkspaceSize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) * 4 + sizeof(cudnnConvolutionBwdDataAlgo_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_DATA_WORKSPACE_SIZE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(wDesc);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(dxDesc);
    reqBuf.Push(algo);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(sizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, wDesc, dyDesc, convDesc, dxDesc, algo, sizeInBytes);
}

cudnnStatus_t cudnnConvolutionBackwardFilter(
              cudnnHandle_t handle, const void *alpha,
              const cudnnTensorDescriptor_t xDesc, const void *x,
              const cudnnTensorDescriptor_t dyDesc,const void *dy,
              const cudnnConvolutionDescriptor_t convDesc,
              cudnnConvolutionBwdFilterAlgo_t algo,
              void *workSpace, size_t workSpaceSizeInBytes,
              const void *beta,
              const cudnnFilterDescriptor_t dwDesc,void *dw) {
    const char* func_name = "cudnnConvolutionBackwardFilter";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const void *, const cudnnTensorDescriptor_t, const void *, const cudnnTensorDescriptor_t, const void *, const cudnnConvolutionDescriptor_t, cudnnConvolutionBwdFilterAlgo_t, void *, size_t, const void *, const cudnnFilterDescriptor_t, void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnConvolutionBackwardFilter"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*6 + sizeof(uint64_t) + sizeof(cudnnConvolutionBwdFilterAlgo_t) + sizeof(uint64_t) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CONVOLUTION_BACKWARD_FILTER);
    reqBuf.Push64BitPointer(handle);
    reqBuf.PushConst((const float*)alpha);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(x);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(dy);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(algo);
    reqBuf.Push64BitPointer(workSpace);
    reqBuf.Push(workSpaceSizeInBytes);
    reqBuf.PushConst((const float*)beta);
    reqBuf.Push64BitPointer(dwDesc);
    reqBuf.Push64BitPointer(dw);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, alpha, xDesc, x, dyDesc, dy, convDesc, algo, workSpace, workSpaceSizeInBytes, beta, dwDesc, dw);
}

cudnnStatus_t cudnnGetConvolutionBackwardFilterWorkspaceSize(
              cudnnHandle_t handle, const cudnnTensorDescriptor_t xDesc,
              const cudnnTensorDescriptor_t dyDesc, 
              const cudnnConvolutionDescriptor_t convDesc,
              const cudnnFilterDescriptor_t dwDesc,
              cudnnConvolutionBwdFilterAlgo_t algo, size_t *sizeInBytes) {
    const char* func_name = "cudnnGetConvolutionBackwardFilterWorkspaceSize";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const cudnnTensorDescriptor_t, const cudnnTensorDescriptor_t, const cudnnConvolutionDescriptor_t, const cudnnFilterDescriptor_t, cudnnConvolutionBwdFilterAlgo_t, size_t *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnGetConvolutionBackwardFilterWorkspaceSize"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + sizeof(uint64_t) * 4 + sizeof(cudnnConvolutionBwdFilterAlgo_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_WORKSPACE_SIZE);
    reqBuf.Push64BitPointer(handle);
    reqBuf.Push64BitPointer(xDesc);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push64BitPointer(dwDesc);
    reqBuf.Push(algo);

    RequestIOV resBuf = RequestIOV();
    resBuf.Push(sizeInBytes);
    clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);

    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, xDesc, dyDesc, convDesc, dwDesc, algo, sizeInBytes);
}

cudnnStatus_t cudnnConvolutionBackwardData(
              cudnnHandle_t handle, const void *alpha,
              const cudnnFilterDescriptor_t wDesc, const void *w,
              const cudnnTensorDescriptor_t dyDesc,const void *dy,
              const cudnnConvolutionDescriptor_t convDesc,
              cudnnConvolutionBwdDataAlgo_t algo,
              void *workSpace, size_t workSpaceSizeInBytes,
              const void *beta,
              const cudnnTensorDescriptor_t dxDesc, void *dx) {
    const char* func_name = "cudnnConvolutionBackwardData";
    HookLog(func_name);
    using func_ptr = cudnnStatus_t (*)(cudnnHandle_t, const void *, const cudnnFilterDescriptor_t, const void *, const cudnnTensorDescriptor_t, const void *, const cudnnConvolutionDescriptor_t, cudnnConvolutionBwdDataAlgo_t, void *, size_t, const void *, const cudnnTensorDescriptor_t, void *);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(RTLD_NEXT, "cudnnConvolutionBackwardData"));

    // RequestBuffer reqBuf = RequestBuffer(sizeof(uint64_t) + (sizeof(size_t)+sizeof(const float))*2 + sizeof(uint64_t)*6 + sizeof(uint64_t) + sizeof(cudnnConvolutionBwdDataAlgo_t) + sizeof(uint64_t) + sizeof(size_t));
    RequestIOV reqBuf = RequestIOV();
    reqBuf.PushRequestType(CUDNN_CONVOLUTION_BACKWARD_DATA);
    reqBuf.Push64BitPointer(handle);
    reqBuf.PushConst((const float*)alpha);
    reqBuf.Push64BitPointer(wDesc);
    reqBuf.Push64BitPointer(w);
    reqBuf.Push64BitPointer(dyDesc);
    reqBuf.Push64BitPointer(dy);
    reqBuf.Push64BitPointer(convDesc);
    reqBuf.Push(algo);
    reqBuf.Push64BitPointer(workSpace);
    reqBuf.Push(workSpaceSizeInBytes);
    reqBuf.PushConst((const float*)beta);
    reqBuf.Push64BitPointer(dxDesc);
    reqBuf.Push64BitPointer(dx);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    clientEpObj->SendRequest(&reqBuf);
    return CUDNN_STATUS_SUCCESS;

    //return func_entry(handle, alpha, wDesc, w, dyDesc, dy, convDesc, algo, workSpace, workSpaceSizeInBytes, beta, dxDesc, dx);
}
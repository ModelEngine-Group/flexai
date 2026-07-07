#include "../../include/serverEndpoint.h"

static const char* myName = "cuDNNHandle";

DEFINE_SERVER_AM_CALLBACK(cudnnCreateHandle) {
    tool::Logging(myName, "CUDNN_CREATE\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    uint64_t            virAddr     = reqBuf.Pop<uint64_t>();
    cudnnHandle_t       handle      = NULL;
    cudnnStatus_t       exit_code   = cudnnCreate(&handle);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnCreate success, handle = %p\n", handle);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, handle);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDNN_CREATE);
            handle = (cudnnHandle_t)serverEp->GetHandleVirAddr(handle, CUDNN_CREATE);
            resBuf.Push64BitPointer(handle);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnCreate failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnDestroyHandle) {
    tool::Logging(myName, "CUDNN_DESTROY\n");
    ServerEndpoint*     serverEp    = (ServerEndpoint*) arg;
    RequestIOV          reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t       handle      = (cudnnHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudnnStatus_t       exit_code   = cudnnDestroy(handle);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnDestroy success, handle = %p\n", handle);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnDestroy failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnCreateTensorDescriptorHandle) {
    tool::Logging(myName, "CUDNN_CREATE_TENSOR_DESCRIPTOR\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    uint64_t                virAddr     = reqBuf.Pop<uint64_t>();
    cudnnTensorDescriptor_t tensorDesc = NULL;
    cudnnStatus_t           exit_code   = cudnnCreateTensorDescriptor(&tensorDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnCreateTensorDescriptor success, tensorDesc = %p\n", tensorDesc);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, tensorDesc);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDNN_CREATE_TENSOR_DESCRIPTOR);
            tensorDesc = (cudnnTensorDescriptor_t)serverEp->GetHandleVirAddr(tensorDesc, CUDNN_CREATE_TENSOR_DESCRIPTOR);
            resBuf.Push64BitPointer(tensorDesc);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnCreateTensorDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnDestroyTensorDescriptorHandle) {
    tool::Logging(myName, "CUDNN_DESTROY_TENSOR_DESCRIPTOR\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnTensorDescriptor_t tensorDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudnnStatus_t           exit_code   = cudnnDestroyTensorDescriptor(tensorDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnDestroyTensorDescriptor success, tensorDesc = %p\n", tensorDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnDestroyTensorDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetTensorSizeInBytesHandle) {
    tool::Logging(myName, "CUDNN_GET_TENSOR_SIZE_IN_BYTES\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnTensorDescriptor_t tensorDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    size_t                  sizeInBytes = 0;
    cudnnStatus_t           exit_code   = cudnnGetTensorSizeInBytes(tensorDesc, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetTensorSizeInBytes success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_TENSOR_SIZE_IN_BYTES);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetTensorSizeInBytes failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetTensor4dDescriptorHandle) {
    tool::Logging(myName, "CUDNN_SET_TENSOR_4D_DESCRIPTOR\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnTensorDescriptor_t tensorDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorFormat_t     format     = reqBuf.Pop<cudnnTensorFormat_t>();
    cudnnDataType_t         dataType   = reqBuf.Pop<cudnnDataType_t>();
    int                     n          = reqBuf.Pop<int>();
    int                     c          = reqBuf.Pop<int>();
    int                     h          = reqBuf.Pop<int>();
    int                     w          = reqBuf.Pop<int>();
    cudnnStatus_t           exit_code  = cudnnSetTensor4dDescriptor(tensorDesc, format, dataType, n, c, h, w);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetTensor4dDescriptor success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetTensor4dDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetTensorNdDescriptorHandle) {
    tool::Logging(myName, "CUDNN_SET_TENSOR_ND_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnTensorDescriptor_t tensorDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnDataType_t         dataType   = reqBuf.Pop<cudnnDataType_t>();
    int                     nbDims     = reqBuf.Pop<int>();
    const int*              dimA       = reqBuf.AssignAddr<const int>();
    const int*              strideA    = reqBuf.AssignAddr<const int>();
    cudnnStatus_t           exit_code  = cudnnSetTensorNdDescriptor(tensorDesc, dataType, nbDims, dimA, strideA);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetTensorNdDescriptor success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetTensorNdDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetTensorNdDescriptorExHandle) {
    tool::Logging(myName, "CUDNN_SET_TENSOR_ND_DESCRIPTOR_EX\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnTensorDescriptor_t tensorDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorFormat_t     format     = reqBuf.Pop<cudnnTensorFormat_t>();
    cudnnDataType_t         dataType   = reqBuf.Pop<cudnnDataType_t>();
    int                     nbDims     = reqBuf.Pop<int>();
    const int*              dimA       = reqBuf.AssignAddr<const int>();
    cudnnStatus_t           exit_code  = cudnnSetTensorNdDescriptorEx(tensorDesc, format, dataType, nbDims, dimA);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetTensorNdDescriptorEx success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetTensorNdDescriptorEx failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnCreateTensorTransformDescriptorHandle) {
    tool::Logging(myName, "CUDNN_CREATE_TENSOR_TRANSFORM_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    uint64_t                virAddr    = reqBuf.Pop<uint64_t>();
    cudnnTensorTransformDescriptor_t transformDesc = NULL;
    cudnnStatus_t           exit_code  = cudnnCreateTensorTransformDescriptor(&transformDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnCreateTensorTransformDescriptor success, transformDesc = %p\n", transformDesc);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, transformDesc);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDNN_CREATE_TENSOR_TRANSFORM_DESCRIPTOR);
            transformDesc = (cudnnTensorTransformDescriptor_t)serverEp->GetHandleVirAddr(transformDesc, CUDNN_CREATE_TENSOR_TRANSFORM_DESCRIPTOR);
            resBuf.Push64BitPointer(transformDesc);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnCreateTensorTransformDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetTensorTransformDescriptorHandle) {
    tool::Logging(myName, "CUDNN_SET_TENSOR_TRANSFORM_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnTensorTransformDescriptor_t transformDesc = (cudnnTensorTransformDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const uint32_t         nbDims      = reqBuf.Pop<const uint32_t>();
    const cudnnTensorFormat_t destFormat = reqBuf.Pop<const cudnnTensorFormat_t>();
    const int32_t*         padBeforeA  = reqBuf.AssignAddr<const int32_t>();
    const int32_t*         padAfterA   = reqBuf.AssignAddr<const int32_t>();
    const uint32_t*        foldA       = reqBuf.AssignAddr<const uint32_t>(); // spatial dimension (dimensions 2 and up)
    const cudnnFoldingDirection_t dirt = reqBuf.Pop<const cudnnFoldingDirection_t>();
    cudnnStatus_t           exit_code  = cudnnSetTensorTransformDescriptor(transformDesc, nbDims, destFormat, padBeforeA, padAfterA, foldA, dirt);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetTensorTransformDescriptor success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetTensorTransformDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnDestroyTensorTransformDescriptorHandle) {
    tool::Logging(myName, "CUDNN_DESTROY_TENSOR_TRANSFORM_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnTensorTransformDescriptor_t transformDesc = (cudnnTensorTransformDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudnnStatus_t           exit_code  = cudnnDestroyTensorTransformDescriptor(transformDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnDestroyTensorTransformDescriptor success, transformDesc = %p\n", transformDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnDestroyTensorTransformDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnInitTransformDestHandle) {
    tool::Logging(myName, "CUDNN_INIT_TRANSFORM_DEST\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnTensorTransformDescriptor_t transformDesc = (cudnnTensorTransformDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t srcDesc    = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t destDesc   = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    size_t                  destSizeInBytes = 0;
    cudnnStatus_t           exit_code  = cudnnInitTransformDest(transformDesc, srcDesc, destDesc, &destSizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnInitTransformDest success, destSizeInBytes = %lu, transformDesc = %p, srcDesc = %p, destDesc = %p\n", destSizeInBytes, transformDesc, srcDesc, destDesc);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_INIT_TRANSFORM_DEST);
        resBuf.Push(destSizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnInitTransformDest failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }

}

DEFINE_SERVER_AM_CALLBACK(cudnnTransformTensorExHandle) {
    tool::Logging(myName, "CUDNN_TRANSFORM_TENSOR_EX\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)                         serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorTransformDescriptor_t transDesc 
                                        = (const cudnnTensorTransformDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             alpha       = reqBuf.AssignAddr<const float>();
    const cudnnTensorDescriptor_t srDesc= (const cudnnTensorDescriptor_t)         serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             srcData     = (const void*)                           serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             beta        = reqBuf.AssignAddr<const float>();
    const cudnnTensorDescriptor_t dsDesc= (const cudnnTensorDescriptor_t)         serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   destData    = (void*)                                 serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnStatus_t           exit_code   = cudnnTransformTensorEx(handle, transDesc, alpha, srDesc, srcData, beta, dsDesc, destData);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnTransformTensorEx success, handle = %p, transDesc = %p, srDesc = %p, dsDesc = %p\n", handle, transDesc, srDesc, dsDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnTransformTensorEx failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnTransformFilterHandle) {
    tool::Logging(myName, "CUDNN_TRANSFORM_FILTER\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)                         serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorTransformDescriptor_t transDesc 
                                        = (const cudnnTensorTransformDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             alpha       = reqBuf.AssignAddr<const float>();
    const cudnnFilterDescriptor_t srDesc= (const cudnnFilterDescriptor_t)         serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             srcData     = (const void*)                           serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             beta        = reqBuf.AssignAddr<const float>();
    const cudnnFilterDescriptor_t dsDesc= (const cudnnFilterDescriptor_t)         serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   destData    = (void*)                                 serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnStatus_t           exit_code   = cudnnTransformFilter(handle, transDesc, alpha, srDesc, srcData, beta, dsDesc, destData);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnTransformFilter success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnTransformFilter failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnCreateFilterDescriptorHandle) {
    tool::Logging(myName, "CUDNN_CREATE_FILTER_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    uint64_t                virAddr    = reqBuf.Pop<uint64_t>();
    cudnnFilterDescriptor_t filterDesc = NULL;
    cudnnStatus_t           exit_code  = cudnnCreateFilterDescriptor(&filterDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnCreateFilterDescriptor success, filterDesc = %p\n", filterDesc);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, filterDesc);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDNN_CREATE_FILTER_DESCRIPTOR);
            filterDesc = (cudnnFilterDescriptor_t)serverEp->GetHandleVirAddr(filterDesc, CUDNN_CREATE_FILTER_DESCRIPTOR);
            resBuf.Push64BitPointer(filterDesc);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnCreateFilterDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetFilterNdDescriptorHandle) {
    tool::Logging(myName, "CUDNN_SET_FILTER_ND_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnFilterDescriptor_t filterDesc = (cudnnFilterDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnDataType_t         dataType   = reqBuf.Pop<cudnnDataType_t>();
    cudnnTensorFormat_t     format     = reqBuf.Pop<cudnnTensorFormat_t>();
    int                     nbDims     = reqBuf.Pop<int>();
    const int*              filterDimA = reqBuf.AssignAddr<const int>();
    cudnnStatus_t           exit_code  = cudnnSetFilterNdDescriptor(filterDesc, dataType, format, nbDims, filterDimA);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetFilterNdDescriptor success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetFilterNdDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnDestroyFilterDescriptorHandle) {
    tool::Logging(myName, "CUDNN_DESTROY_FILTER_DESCRIPTOR\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnFilterDescriptor_t filterDesc = (cudnnFilterDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudnnStatus_t           exit_code  = cudnnDestroyFilterDescriptor(filterDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnDestroyFilterDescriptor success, filterDesc = %p\n", filterDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnDestroyFilterDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetFilterSizeInBytesHandle) {
    tool::Logging(myName, "CUDNN_GET_FILTER_SIZE_IN_BYTES\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnFilterDescriptor_t filterDesc = (cudnnFilterDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    size_t                  sizeInBytes= 0;
    cudnnStatus_t           exit_code  = cudnnGetFilterSizeInBytes(filterDesc, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetFilterSizeInBytes success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_FILTER_SIZE_IN_BYTES);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetFilterSizeInBytes failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetFoldedConvBackwardDataDescriptorsHandle) {
    tool::Logging(myName, "CUDNN_GET_FOLDED_CONV_BACKWARD_DATA_DESCRIPTORS\n");
    ServerEndpoint*                  serverEp            = (ServerEndpoint*) arg;
    RequestIOV                       reqBuf              = RequestIOV(header, header_length, data);
    cudnnHandle_t                    handle              = (cudnnHandle_t)                    serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnFilterDescriptor_t    filterDesc          = (const cudnnFilterDescriptor_t)    serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t    diffDesc            = (const cudnnTensorDescriptor_t)    serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc          = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t    gradDesc            = (const cudnnTensorDescriptor_t)    serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorFormat_t        transformFormat     = reqBuf.Pop<cudnnTensorFormat_t>();
    cudnnFilterDescriptor_t          foldedFilterDesc    = (cudnnFilterDescriptor_t)          serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t          paddedDiffDesc      = (cudnnTensorDescriptor_t)          serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionDescriptor_t     foldedConvDesc      = (cudnnConvolutionDescriptor_t)     serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t          foldedGradDesc      = (cudnnTensorDescriptor_t)          serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorTransformDescriptor_t filterFoldTransDesc = (cudnnTensorTransformDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorTransformDescriptor_t diffPadTransDesc    = (cudnnTensorTransformDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorTransformDescriptor_t gradFoldTransDesc   = (cudnnTensorTransformDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorTransformDescriptor_t gradUnfoldTransDesc = (cudnnTensorTransformDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnStatus_t                    exit_code           = cudnnGetFoldedConvBackwardDataDescriptors(handle, filterDesc, diffDesc, convDesc, gradDesc, transformFormat, foldedFilterDesc, paddedDiffDesc, foldedConvDesc, foldedGradDesc, filterFoldTransDesc, diffPadTransDesc, gradFoldTransDesc, gradUnfoldTransDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetFoldedConvBackwardDataDescriptors success, handle = %p, filterDesc = %p, diffDesc = %p, convDesc = %p, gradDesc = %p, transformFormat = %d, foldedFilterDesc = %p, paddedDiffDesc = %p, foldedConvDesc = %p, foldedGradDesc = %p, filterFoldTransDesc = %p, diffPadTransDesc = %p, gradFoldTransDesc = %p, gradUnfoldTransDesc = %p\n", handle, filterDesc, diffDesc, convDesc, gradDesc, transformFormat, foldedFilterDesc, paddedDiffDesc, foldedConvDesc, foldedGradDesc, filterFoldTransDesc, diffPadTransDesc, gradFoldTransDesc, gradUnfoldTransDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetFoldedConvBackwardDataDescriptors failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetStreamHandle) {
    tool::Logging(myName, "CUDNN_SET_STREAM\n");
    ServerEndpoint*         serverEp= (ServerEndpoint*) arg;
    RequestIOV              reqBuf  = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle  = (cudnnHandle_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudaStream_t            stream  = (cudaStream_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    cudnnStatus_t           exit_code = cudnnSetStream(handle, stream);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetStream success, handle = %p, stream = %p\n", handle, stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetStream failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBatchNormalizationBackwardExHandle) {
    tool::Logging(myName, "CUDNN_BATCH_NORMALIZATION_BACKWARD_EX\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBatchNormMode_t    mode        = reqBuf.Pop<cudnnBatchNormMode_t>();
    cudnnBatchNormOps_t     bnOps       = reqBuf.Pop<cudnnBatchNormOps_t>();
    const void*             alphaDataDiff = reqBuf.AssignAddr<const float>(); // always one element
    const void*             betaDataDiff  = reqBuf.AssignAddr<const float>(); // always one element
    const void*             alphaParamDiff= reqBuf.AssignAddr<const float>(); // always one element
    const void*             betaParamDiff = reqBuf.AssignAddr<const float>(); // always one element
    cudnnTensorDescriptor_t xDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             xData       = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t yDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             yData       = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dyDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             dyData      = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dzDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   dzData      = (void*)        serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dxDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   dxData      = (void*)        serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dBnScaleBiasDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             bnScaleData = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             bnBiasData  = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    void*                   dBnScaleData= (void*)        serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    void*                   dBnBiasData = (void*)        serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    double                  epsilon     = reqBuf.Pop<double>();
    const void*             savedMean   = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             savedInvVariance = (const void*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnActivationDescriptor_t activationDesc = (cudnnActivationDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   workSpace   = (void*)       serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                  workSpaceSizeInBytes = reqBuf.Pop<size_t>();
    void*                   reserveSpace= (void*)       serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                  reserveSpaceSizeInBytes = reqBuf.Pop<size_t>();
    cudnnStatus_t           exit_code   = cudnnBatchNormalizationBackwardEx(handle, mode, bnOps,
                                          alphaDataDiff, betaDataDiff, alphaParamDiff, betaParamDiff, 
                                          xDesc, xData, yDesc, yData, dyDesc, dyData, dzDesc, dzData, dxDesc, dxData, 
                                          dBnScaleBiasDesc, bnScaleData, bnBiasData, dBnScaleData, dBnBiasData, 
                                          epsilon, savedMean, savedInvVariance, activationDesc, 
                                          workSpace, workSpaceSizeInBytes, reserveSpace, reserveSpaceSizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBatchNormalizationBackwardEx success, handle = %p, xDesc = %p, yDesc = %p, dyDesc = %p, dzDesc = %p, dxDesc = %p, dBnScaleBiasDesc = %p\n", handle, xDesc, yDesc, dyDesc, dzDesc, dxDesc, dBnScaleBiasDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBatchNormalizationBackwardEx failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBatchNormalizationForwardTrainingExHandle) {
    tool::Logging(myName, "CUDNN_BATCH_NORMALIZATION_FORWARD_TRAINING_EX\n");
    ServerEndpoint*         serverEp   = (ServerEndpoint*) arg;
    RequestIOV              reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBatchNormMode_t    mode        = reqBuf.Pop<cudnnBatchNormMode_t>();
    cudnnBatchNormOps_t     bnOps       = reqBuf.Pop<cudnnBatchNormOps_t>();
    const float*            alphaData   = reqBuf.AssignAddr<const float>(); // always one element
    const float*            betaData    = reqBuf.AssignAddr<const float>(); // always one element
    cudnnTensorDescriptor_t xDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             xData       = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t zDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             zData       = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t yDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   yData       =                serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             bnScaleData = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             bnBiasData  = (const void*)  serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    double                  exponentialAverageFactor = reqBuf.Pop<double>();
    void*                   resultRunningMean =          serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    void*                   resultRunningVariance =      serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    double                  epsilon     = reqBuf.Pop<double>();
    void*                   saveMean    =                serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    void*                   saveInvVari =                serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnActivationDescriptor_t activationDesc = (cudnnActivationDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   workSpace   =                serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                  workSpaceSizeInBytes = reqBuf.Pop<size_t>();
    void*                   reserveSpace=                serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                  reserveSpaceSizeInBytes = reqBuf.Pop<size_t>();
    cudnnStatus_t           exit_code   = cudnnBatchNormalizationForwardTrainingEx(handle, mode, bnOps, alphaData, betaData, xDesc, xData, zDesc, zData, yDesc, yData, bnScaleBiasMeanVarDesc, bnScaleData, bnBiasData, exponentialAverageFactor, resultRunningMean, resultRunningVariance, epsilon, saveMean, saveInvVari, activationDesc, workSpace, workSpaceSizeInBytes, reserveSpace, reserveSpaceSizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBatchNormalizationForwardTrainingEx success, handle = %p, xDesc = %p, zDesc = %p, yDesc = %p, bnScaleBiasMeanVarDesc = %p\n", handle, xDesc, zDesc, yDesc, bnScaleBiasMeanVarDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBatchNormalizationForwardTrainingEx failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBatchNormalizationForwardInferenceHandle) {
    tool::Logging(myName, "CUDNN_BATCH_NORMALIZATION_FORWARD_INFERENCE\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBatchNormMode_t    mode        = reqBuf.Pop<cudnnBatchNormMode_t>();
    const float*            alpha       = reqBuf.AssignAddr<const float>(); // always one element
    const float*            beta        = reqBuf.AssignAddr<const float>(); // always one element
    cudnnTensorDescriptor_t xDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             x           = (const void*)   serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t yDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                   y           = (void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc = (cudnnTensorDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*             bnScale     = (const void*)   serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             bnBias      = (const void*)   serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             estimatedMean = (const void*) serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const void*             estimatedVariance = (const void*)serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    double                  epsilon     = reqBuf.Pop<double>();
    cudnnStatus_t           exit_code   = cudnnBatchNormalizationForwardInference(handle, mode, alpha, beta, xDesc, x, yDesc, y, bnScaleBiasMeanVarDesc, bnScale, bnBias, estimatedMean, estimatedVariance, epsilon);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBatchNormalizationForwardInference success, handle = %p, xDesc = %p, yDesc = %p, bnScaleBiasMeanVarDesc = %p\n", handle, xDesc, yDesc, bnScaleBiasMeanVarDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBatchNormalizationForwardInference failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBackendCreateDescriptorHandle) {
    tool::Logging(myName, "CUDNN_BACKEND_CREATE_DESCRIPTOR\n");
    ServerEndpoint*          serverEp    = (ServerEndpoint*) arg;
    RequestIOV               reqBuf      = RequestIOV(header, header_length, data);
    cudnnBackendDescriptorType_t decType = reqBuf.Pop<cudnnBackendDescriptorType_t>();
    uint64_t                 virAddr     = reqBuf.Pop<uint64_t>();
    cudnnBackendDescriptor_t backendDesc = NULL;
    cudnnStatus_t            exit_code   = cudnnBackendCreateDescriptor(decType, &backendDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBackendCreateDescriptor success, backendDesc = %p\n", backendDesc);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, backendDesc);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDNN_BACKEND_CREATE_DESCRIPTOR);
            backendDesc = (cudnnBackendDescriptor_t)serverEp->GetHandleVirAddr(backendDesc, CUDNN_BACKEND_CREATE_DESCRIPTOR);
            resBuf.Push64BitPointer(backendDesc);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBackendCreateDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBackendDestroyDescriptorHandle) {
    tool::Logging(myName, "CUDNN_BACKEND_DESTROY_DESCRIPTOR\n");
    ServerEndpoint*          serverEp    = (ServerEndpoint*) arg;
    RequestIOV               reqBuf      = RequestIOV(header, header_length, data);
    cudnnBackendDescriptor_t backendDesc = (cudnnBackendDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudnnStatus_t            exit_code   = cudnnBackendDestroyDescriptor(backendDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBackendDestroyDescriptor success, backendDesc = %p\n", backendDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBackendDestroyDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBackendSetAttributeHandle) { // todo: to be validated
    tool::Logging(myName, "CUDNN_BACKEND_SET_ATTRIBUTE\n");
    ServerEndpoint*             serverEp = (ServerEndpoint*) arg;
    RequestIOV                  reqBuf   = RequestIOV(header, header_length, data);
    cudnnBackendDescriptor_t backendDesc = (cudnnBackendDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBackendAttributeName_t attrName = reqBuf.Pop<cudnnBackendAttributeName_t>();
    cudnnBackendAttributeType_t attrType = reqBuf.Pop<cudnnBackendAttributeType_t>();
    int64_t                     eleCount = reqBuf.Pop<int64_t>();
    const uint8_t*           arrayOfElem = reqBuf.AssignAddrForAll<const uint8_t>(); // size is eleCount * sizeof(...)

    if (attrType == CUDNN_TYPE_BACKEND_DESCRIPTOR || attrType == CUDNN_TYPE_HANDLE 
     || attrName == CUDNN_ATTR_EXECUTION_PLAN_COMPUTED_INTERMEDIATE_UIDS
     || attrName == CUDNN_ATTR_EXECUTION_PLAN_RUN_ONLY_INTERMEDIATE_UIDS
     || attrName == CUDNN_ATTR_INTERMEDIATE_INFO_UNIQUE_ID
     || attrName == CUDNN_ATTR_INTERMEDIATE_INFO_DEPENDENT_DATA_UIDS
     || attrName == CUDNN_ATTR_TENSOR_UNIQUE_ID
     || attrName == CUDNN_ATTR_VARIANT_PACK_UNIQUE_IDS
     || attrName == CUDNN_ATTR_LAYOUT_INFO_TENSOR_UID
    ) {
        uint64_t* ptrList = (uint64_t*)arrayOfElem;
        for (int i = 0; i < eleCount; i++) {
            ptrList[i] = (uint64_t)serverEp->GetHandle(ptrList[i]);
        }
    }
    
    if (attrType == CUDNN_TYPE_VOID_PTR) {
        uint64_t* ptrList = (uint64_t*)arrayOfElem;
        for (int i = 0; i < eleCount; i++) {
            ptrList[i] = (uint64_t)serverEp->GetDevPtr(ptrList[i]);
        }
    }

    cudnnStatus_t            exit_code   = cudnnBackendSetAttribute(backendDesc, attrName, attrType, eleCount, arrayOfElem);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBackendSetAttribute success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBackendSetAttribute failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBackendGetAttributeHandle) { // todo: to be validated
    tool::Logging(myName, "CUDNN_BACKEND_GET_ATTRIBUTE\n");
    ServerEndpoint*          serverEp    = (ServerEndpoint*) arg;
    RequestIOV               reqBuf      = RequestIOV(header, header_length, data);
    cudnnBackendDescriptor_t backendDesc = (cudnnBackendDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBackendAttributeName_t attrName = reqBuf.Pop<cudnnBackendAttributeName_t>();
    cudnnBackendAttributeType_t attrType = reqBuf.Pop<cudnnBackendAttributeType_t>();
    int64_t                 reqElemCount = reqBuf.Pop<int64_t>();
    size_t                      varLen   = reqBuf.Pop<size_t>();
    // uint8_t*                arrayOfElem  = (uint8_t*)malloc(varLen * reqElemCount);
    uint8_t                arrayOfElem[varLen * reqElemCount];
    int64_t                 elementCount = 0;
    cudnnStatus_t            exit_code   = cudnnBackendGetAttribute(backendDesc, attrName, attrType, reqElemCount, &elementCount, arrayOfElem);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBackendSetAttribute success\n");

        if (attrType == CUDNN_TYPE_BACKEND_DESCRIPTOR || attrType == CUDNN_TYPE_HANDLE 
        || attrName == CUDNN_ATTR_EXECUTION_PLAN_COMPUTED_INTERMEDIATE_UIDS
        || attrName == CUDNN_ATTR_EXECUTION_PLAN_RUN_ONLY_INTERMEDIATE_UIDS
        || attrName == CUDNN_ATTR_INTERMEDIATE_INFO_UNIQUE_ID
        || attrName == CUDNN_ATTR_INTERMEDIATE_INFO_DEPENDENT_DATA_UIDS
        || attrName == CUDNN_ATTR_TENSOR_UNIQUE_ID
        || attrName == CUDNN_ATTR_VARIANT_PACK_UNIQUE_IDS
        || attrName == CUDNN_ATTR_LAYOUT_INFO_TENSOR_UID
        ) {
            uint64_t* ptrList = (uint64_t*)arrayOfElem;
            for (int i = 0; i < elementCount; i++) {
                ptrList[i] = serverEp->FindHandleVirAddr((void*)ptrList[i]);
            }
        }
        
        if (attrType == CUDNN_TYPE_VOID_PTR) {
            uint64_t* ptrList = (uint64_t*)arrayOfElem;
            for (int i = 0; i < elementCount; i++) {
                ptrList[i] = serverEp->FindDevPtrVirAddr((void*)ptrList[i]);
            }
        }

        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_BACKEND_GET_ATTRIBUTE);
        resBuf.Push(elementCount);
        resBuf.Push(arrayOfElem, varLen * reqElemCount);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBackendSetAttribute failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBackendExecuteHandle) {
    tool::Logging(myName, "CUDNN_BACKEND_EXECUTE\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBackendDescriptor_t execPlan   = (cudnnBackendDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBackendDescriptor_t varianPack = (cudnnBackendDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnStatus_t           exit_code   = cudnnBackendExecute(handle, execPlan, varianPack);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBackendExecute success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBackendExecute failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnBackendFinalizeHandle) {
    tool::Logging(myName, "CUDNN_BACKEND_FINALIZE\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnBackendDescriptor_t descriptor = (cudnnBackendDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnStatus_t           exit_code   = cudnnBackendFinalize(descriptor);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnBackendFinalize success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnBackendFinalize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetBatchNormalizationBackwardExWorkspaceSizeHandle) {
    tool::Logging(myName, "CUDNN_GET_BATCH_NORMALIZATION_BACKWARD_EX_WORKSPACE_SIZE\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBatchNormMode_t    mode        = reqBuf.Pop<cudnnBatchNormMode_t>();
    cudnnBatchNormOps_t     bnOps       = reqBuf.Pop<cudnnBatchNormOps_t>();
    cudnnTensorDescriptor_t xDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t yDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dyDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dzDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dxDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t dBnScaleBiasDesc = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnActivationDescriptor_t activationDesc = (cudnnActivationDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    size_t                  sizeInBytes = 0;
    cudnnStatus_t           exit_code   = cudnnGetBatchNormalizationBackwardExWorkspaceSize(handle, mode, bnOps, xDesc, yDesc, dyDesc, dzDesc, dxDesc, dBnScaleBiasDesc, activationDesc, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetBatchNormalizationBackwardExWorkspaceSize success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_BATCH_NORMALIZATION_BACKWARD_EX_WORKSPACE_SIZE);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetBatchNormalizationBackwardExWorkspaceSize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetBatchNormalizationForwardTrainingExWorkspaceSizeHandle) {
    tool::Logging(myName, "CUDNN_GET_BATCH_NORMALIZATION_FORWARD_TRAINING_EX_WORKSPACE_SIZE\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBatchNormMode_t    mode        = reqBuf.Pop<cudnnBatchNormMode_t>();
    cudnnBatchNormOps_t     bnOps       = reqBuf.Pop<cudnnBatchNormOps_t>();
    cudnnTensorDescriptor_t xDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t zDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t yDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t bnScaleBiasMeanVarDesc = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnActivationDescriptor_t activationDesc = (cudnnActivationDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    size_t                  sizeInBytes = 0;
    cudnnStatus_t           exit_code   = cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize(handle, mode, bnOps, xDesc, zDesc, yDesc, bnScaleBiasMeanVarDesc, activationDesc, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_BATCH_NORMALIZATION_FORWARD_TRAINING_EX_WORKSPACE_SIZE);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetBatchNormalizationForwardTrainingExWorkspaceSize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetBatchNormalizationTrainingExReserveSpaceSizeHandle) {
    tool::Logging(myName, "CUDNN_GET_BATCH_NORMALIZATION_TRAINING_EX_RESERVE_SPACE_SIZE\n");
    ServerEndpoint*         serverEp    = (ServerEndpoint*) arg;
    RequestIOV              reqBuf      = RequestIOV(header, header_length, data);
    cudnnHandle_t           handle      = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnBatchNormMode_t    mode        = reqBuf.Pop<cudnnBatchNormMode_t>();
    cudnnBatchNormOps_t     bnOps       = reqBuf.Pop<cudnnBatchNormOps_t>();
    cudnnActivationDescriptor_t activationDesc = (cudnnActivationDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t xDesc       = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    size_t                  sizeInBytes = 0;
    cudnnStatus_t           exit_code   = cudnnGetBatchNormalizationTrainingExReserveSpaceSize(handle, mode, bnOps, activationDesc, xDesc, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetBatchNormalizationTrainingExReserveSpaceSize success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_BATCH_NORMALIZATION_TRAINING_EX_RESERVE_SPACE_SIZE);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetBatchNormalizationTrainingExReserveSpaceSize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnCreateConvolutionDescriptorHandle) {
    tool::Logging(myName, "CUDNN_CREATE_CONVOLUTION_DESCRIPTOR\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    uint64_t                     virAddr    = reqBuf.Pop<uint64_t>();
    cudnnConvolutionDescriptor_t convDesc   = NULL;
    cudnnStatus_t                exit_code  = cudnnCreateConvolutionDescriptor(&convDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnCreateConvolutionDescriptor success, convDesc = %p\n", convDesc);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, convDesc);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDNN_CREATE_CONVOLUTION_DESCRIPTOR);
            convDesc = (cudnnConvolutionDescriptor_t)serverEp->GetHandleVirAddr(convDesc, CUDNN_CREATE_CONVOLUTION_DESCRIPTOR);
            resBuf.Push64BitPointer(convDesc);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;       
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnCreateConvolutionDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnDestroyConvolutionDescriptorHandle) {
    tool::Logging(myName, "CUDNN_DESTROY_CONVOLUTION_DESCRIPTOR\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudnnStatus_t                exit_code  = cudnnDestroyConvolutionDescriptor(convDesc);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnDestroyConvolutionDescriptor success, convDesc = %p\n", convDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnDestroyConvolutionDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetConvolutionGroupCountHandle) {
    tool::Logging(myName, "CUDNN_SET_CONVOLUTION_GROUP_COUNT\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                          groupCount = reqBuf.Pop<int>();
    cudnnStatus_t                exit_code  = cudnnSetConvolutionGroupCount(convDesc, groupCount);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetConvolutionGroupCount success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetConvolutionGroupCount failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetConvolutionMathTypeHandle) {
    tool::Logging(myName, "CUDNN_SET_CONVOLUTION_MATH_TYPE\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnMathType_t              mathType   = reqBuf.Pop<cudnnMathType_t>();
    cudnnStatus_t                exit_code  = cudnnSetConvolutionMathType(convDesc, mathType);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetConvolutionMathType success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetConvolutionMathType failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetConvolutionNdDescriptorHandle) {
    tool::Logging(myName, "CUDNN_SET_CONVOLUTION_ND_DESCRIPTOR\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    int                          arrayLength= reqBuf.Pop<int>();
    const int*                   padA       = reqBuf.AssignAddr<const int>();
    const int*                   filterStrideA = reqBuf.AssignAddr<const int>();
    const int*                   dilationA  = reqBuf.AssignAddr<const int>();
    cudnnConvolutionMode_t       mode       = reqBuf.Pop<cudnnConvolutionMode_t>();
    cudnnDataType_t              dataType   = reqBuf.Pop<cudnnDataType_t>();
    cudnnStatus_t                exit_code  = cudnnSetConvolutionNdDescriptor(convDesc, arrayLength, padA, filterStrideA, dilationA, mode, dataType);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetConvolutionNdDescriptor success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetConvolutionNdDescriptor failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnSetConvolutionReorderTypeHandle) {
    tool::Logging(myName, "CUDNN_SET_CONVOLUTION_REORDER_TYPE\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnReorderType_t           reorderType = reqBuf.Pop<cudnnReorderType_t>();
    cudnnStatus_t                exit_code  = cudnnSetConvolutionReorderType(convDesc, reorderType);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnSetConvolutionReorderType success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnSetConvolutionReorderType failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetConvolutionForwardAlgorithm_v7Handle) {
    tool::Logging(myName, "CUDNN_GET_CONVOLUTION_FORWARD_ALGORITHM_V7\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t      xDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnFilterDescriptor_t      wDesc      = (cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t      yDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const int              requestAlgoCount = reqBuf.Pop<int>();
    int                    returnedAlgoCount = 0;
    // cudnnConvolutionFwdAlgoPerf_t *perfResults = (cudnnConvolutionFwdAlgoPerf_t*)malloc(requestAlgoCount * sizeof(cudnnConvolutionFwdAlgoPerf_t));
    cudnnConvolutionFwdAlgoPerf_t perfResults[requestAlgoCount];
    cudnnStatus_t                exit_code  = cudnnGetConvolutionForwardAlgorithm_v7(handle, xDesc, wDesc, convDesc, yDesc, requestAlgoCount, &returnedAlgoCount, perfResults);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetConvolutionForwardAlgorithm_v7 success, returnedAlgoCount = %d\n", returnedAlgoCount);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_CONVOLUTION_FORWARD_ALGORITHM_V7);
        resBuf.Push(returnedAlgoCount);
        resBuf.Push(perfResults, requestAlgoCount);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetConvolutionForwardAlgorithm_v7 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetConvolutionBackwardFilterAlgorithm_v7Handle) {
    tool::Logging(myName, "CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_ALGORITHM_V7\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t      xDesc      = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t      dyDesc     = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnFilterDescriptor_t      dwDesc     = (cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const int              requestAlgoCount = reqBuf.Pop<int>();
    int                    returnedAlgoCount = 0;
    // cudnnConvolutionBwdFilterAlgoPerf_t *perfResults = (cudnnConvolutionBwdFilterAlgoPerf_t*)malloc(requestAlgoCount * sizeof(cudnnConvolutionBwdFilterAlgoPerf_t));
    cudnnConvolutionBwdFilterAlgoPerf_t perfResults[requestAlgoCount];
    cudnnStatus_t                exit_code  = cudnnGetConvolutionBackwardFilterAlgorithm_v7(handle, xDesc, dyDesc, convDesc, dwDesc, requestAlgoCount, &returnedAlgoCount, perfResults);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetConvolutionBackwardFilterAlgorithm_v7 success, returnedAlgoCount = %d\n", returnedAlgoCount);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_ALGORITHM_V7);
        resBuf.Push(returnedAlgoCount);
        resBuf.Push(perfResults, requestAlgoCount);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetConvolutionBackwardFilterAlgorithm_v7 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetConvolutionBackwardDataAlgorithm_v7Handle) {
    tool::Logging(myName, "CUDNN_GET_CONVOLUTION_BACKWARD_DATA_ALGORITHM_V7\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)           serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnFilterDescriptor_t      wDesc      = (cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t      dyDesc     = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionDescriptor_t convDesc   = (cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnTensorDescriptor_t      dxDesc     = (cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const int              requestAlgoCount = reqBuf.Pop<int>();
    int                    returnedAlgoCount = 0;
    // cudnnConvolutionBwdDataAlgoPerf_t *perfResults = (cudnnConvolutionBwdDataAlgoPerf_t*)malloc(requestAlgoCount * sizeof(cudnnConvolutionBwdDataAlgoPerf_t));
    cudnnConvolutionBwdDataAlgoPerf_t perfResults[requestAlgoCount];
    cudnnStatus_t                exit_code  = cudnnGetConvolutionBackwardDataAlgorithm_v7(handle, wDesc, dyDesc, convDesc, dxDesc, requestAlgoCount, &returnedAlgoCount, perfResults);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetConvolutionBackwardDataAlgorithm_v7 success, returnedAlgoCount = %d\n", returnedAlgoCount);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_DATA_ALGORITHM_V7);
        resBuf.Push(returnedAlgoCount);
        resBuf.Push(perfResults, requestAlgoCount);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetConvolutionBackwardDataAlgorithm_v7 failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetConvolutionForwardWorkspaceSizeHandle) {
    tool::Logging(myName, "CUDNN_GET_CONVOLUTION_FORWARD_WORKSPACE_SIZE\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)                 serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t xDesc     = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnFilterDescriptor_t wDesc     = (const cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t yDesc     = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionFwdAlgo_t    algo       = reqBuf.Pop<cudnnConvolutionFwdAlgo_t>();
    size_t                       sizeInBytes = 0;
    cudnnStatus_t                exit_code  = cudnnGetConvolutionForwardWorkspaceSize(handle, xDesc, wDesc, convDesc, yDesc, algo, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetConvolutionForwardWorkspaceSize success, sizeInBytes = %lu\n", sizeInBytes);    
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_CONVOLUTION_FORWARD_WORKSPACE_SIZE);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;    
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetConvolutionForwardWorkspaceSize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnConvolutionForwardHandle) {
    tool::Logging(myName, "CUDNN_CONVOLUTION_FORWARD\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)                 serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  alpha      = reqBuf.AssignAddr<const float>(); // always one element
    const cudnnTensorDescriptor_t xDesc     = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  x          = (const void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const cudnnFilterDescriptor_t wDesc     = (const cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  w          = (const void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionFwdAlgo_t    algo       = reqBuf.Pop<cudnnConvolutionFwdAlgo_t>();
    void*                        workSpace  = (void*)               serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                       workSpaceSizeInBytes = reqBuf.Pop<size_t>();
    const void*                  beta       = reqBuf.AssignAddr<const float>(); // always one element
    const cudnnTensorDescriptor_t yDesc     = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                        y          = (void*)               serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnStatus_t                exit_code  = cudnnConvolutionForward(handle, alpha, xDesc, x, wDesc, w, convDesc, algo, workSpace, workSpaceSizeInBytes, beta, yDesc, y);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnConvolutionForward success, handle = %p, xDesc = %p, wDesc = %p, convDesc = %p, yDesc = %p\n", handle, xDesc, wDesc, convDesc, yDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnConvolutionForward failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetConvolutionBackwardDataWorkspaceSizeHandle) {
    tool::Logging(myName, "CUDNN_GET_CONVOLUTION_BACKWARD_DATA_WORKSPACE_SIZE\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)                 serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnFilterDescriptor_t wDesc     = (const cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t dyDesc    = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t dxDesc    = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionBwdDataAlgo_t algo      = reqBuf.Pop<cudnnConvolutionBwdDataAlgo_t>();
    size_t                       sizeInBytes = 0;
    cudnnStatus_t                exit_code  = cudnnGetConvolutionBackwardDataWorkspaceSize(handle, wDesc, dyDesc, convDesc, dxDesc, algo, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetConvolutionBackwardDataWorkspaceSize success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_DATA_WORKSPACE_SIZE);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK; 
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetConvolutionBackwardDataWorkspaceSize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnConvolutionBackwardFilterHandle) {
    tool::Logging(myName, "CUDNN_CONVOLUTION_BACKWARD_FILTER\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)                 serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  alpha      = reqBuf.AssignAddr<const float>(); // always one element
    const cudnnTensorDescriptor_t xDesc     = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  x          = (const void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t dyDesc    = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  dy         = (const void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionBwdFilterAlgo_t algo    = reqBuf.Pop<cudnnConvolutionBwdFilterAlgo_t>();
    void*                        workSpace  =                       serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                       workSpaceSizeInBytes = reqBuf.Pop<size_t>();
    const void*                  beta       = reqBuf.AssignAddr<const float>(); // always one element
    const cudnnFilterDescriptor_t dwDesc    = (const cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                        dw         =                       serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnStatus_t                exit_code  = cudnnConvolutionBackwardFilter(handle, alpha, xDesc, x, dyDesc, dy, convDesc, algo, workSpace, workSpaceSizeInBytes, beta, dwDesc, dw);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnConvolutionBackwardFilter success, handle = %p, xDesc = %p, dyDesc = %p, convDesc = %p, dwDesc = %p\n", handle, xDesc, dyDesc, convDesc, dwDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnConvolutionBackwardFilter failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnGetConvolutionBackwardFilterWorkspaceSizeHandle) {
    tool::Logging(myName, "CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_WORKSPACE_SIZE\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)                 serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t xDesc     = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t dyDesc    = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const cudnnFilterDescriptor_t dwDesc    = (const cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionBwdFilterAlgo_t algo    = reqBuf.Pop<cudnnConvolutionBwdFilterAlgo_t>();
    size_t                       sizeInBytes = 0;
    cudnnStatus_t                exit_code  = cudnnGetConvolutionBackwardFilterWorkspaceSize(handle, xDesc, dyDesc, convDesc, dwDesc, algo, &sizeInBytes);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnGetConvolutionBackwardFilterWorkspaceSize success, sizeInBytes = %lu\n", sizeInBytes);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDNN_GET_CONVOLUTION_BACKWARD_FILTER_WORKSPACE_SIZE);
        resBuf.Push(sizeInBytes);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;         
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnGetConvolutionBackwardFilterWorkspaceSize failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudnnConvolutionBackwardDataHandle) {
    tool::Logging(myName, "CUDNN_CONVOLUTION_BACKWARD_DATA\n");
    ServerEndpoint*              serverEp   = (ServerEndpoint*) arg;
    RequestIOV                   reqBuf     = RequestIOV(header, header_length, data);
    cudnnHandle_t                handle     = (cudnnHandle_t)                 serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  alpha      = reqBuf.AssignAddr<const float>(); // always one element
    const cudnnFilterDescriptor_t wDesc     = (const cudnnFilterDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  w          = (const void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const cudnnTensorDescriptor_t dyDesc    = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    const void*                  dy         = (const void*)         serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    const cudnnConvolutionDescriptor_t convDesc = (const cudnnConvolutionDescriptor_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    cudnnConvolutionBwdDataAlgo_t algo      = reqBuf.Pop<cudnnConvolutionBwdDataAlgo_t>();
    void*                        workSpace  =                       serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    size_t                       workSpaceSizeInBytes = reqBuf.Pop<size_t>();
    const void*                  beta       = reqBuf.AssignAddr<const float>(); // always one element
    const cudnnTensorDescriptor_t dxDesc    = (const cudnnTensorDescriptor_t) serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    void*                        dx         =                       serverEp->GetDevPtr(reqBuf.Pop<uint64_t>());
    cudnnStatus_t                exit_code  = cudnnConvolutionBackwardData(handle, alpha, wDesc, w, dyDesc, dy, convDesc, algo, workSpace, workSpaceSizeInBytes, beta, dxDesc, dx);
    if (exit_code == CUDNN_STATUS_SUCCESS) {
        tool::Logging(myName, "cudnnConvolutionBackwardData success, handle = %p, wDesc = %p, dyDesc = %p, convDesc = %p, dxDesc = %p\n", handle, wDesc, dyDesc, convDesc, dxDesc);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudnnConvolutionBackwardData failed: %d\n", exit_code);
        return UCS_ERR_IO_ERROR;
    }
}
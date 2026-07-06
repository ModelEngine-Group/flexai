#include <cudnn.h>
#include <iostream>

#define checkCUDNN(expression)                                 \
{                                                              \
  cudnnStatus_t status = (expression);                         \
  if (status != CUDNN_STATUS_SUCCESS) {                        \
    std::cerr << "Error on line " << __LINE__ << ": "          \
              << cudnnGetErrorString(status) << std::endl;     \
    std::exit(EXIT_FAILURE);                                   \
  }                                                            \
}

int main() {
    cudnnHandle_t cudnn;
    checkCUDNN(cudnnCreate(&cudnn));

    // Tensor Descriptor for src and dest
    cudnnTensorDescriptor_t srcDesc, destDesc;
    checkCUDNN(cudnnCreateTensorDescriptor(&srcDesc));
    checkCUDNN(cudnnCreateTensorDescriptor(&destDesc));
    int srcDimA[4] = {512, 256, 1, 1};
    checkCUDNN(cudnnSetTensorNdDescriptorEx(srcDesc, CUDNN_TENSOR_NCHW, CUDNN_DATA_FLOAT, 4, srcDimA));

    // Filter Descriptor
    cudnnFilterDescriptor_t filterDesc;
    checkCUDNN(cudnnCreateFilterDescriptor(&filterDesc));
    int filterDimA[4] = {512, 1024, 1, 1};
    checkCUDNN(cudnnSetFilterNdDescriptor(filterDesc, CUDNN_DATA_FLOAT, CUDNN_TENSOR_NCHW, 4, filterDimA));

    // Tensor Transform Descriptor
    cudnnTensorTransformDescriptor_t transformDesc;
    checkCUDNN(cudnnCreateTensorTransformDescriptor(&transformDesc));
    int padBeforeA[4] = {0, 0, 0, 0};
    int padAfterA[4] = {0, 0, 1, 1};
    uint32_t foldA[2] = {2, 2}; // Considering spatial dimensions only as per documentation
    checkCUDNN(cudnnSetTensorTransformDescriptor(transformDesc, 4, CUDNN_TENSOR_NCHW, padBeforeA, padAfterA, foldA, CUDNN_TRANSFORM_FOLD));

    // Initialize dest tensor descriptor based on transformation
    size_t destSizeInBytes;
    checkCUDNN(cudnnInitTransformDest(transformDesc, srcDesc, destDesc, &destSizeInBytes));

    // Clean up
    checkCUDNN(cudnnDestroyTensorDescriptor(srcDesc));
    checkCUDNN(cudnnDestroyTensorDescriptor(destDesc));
    checkCUDNN(cudnnDestroyFilterDescriptor(filterDesc));
    checkCUDNN(cudnnDestroyTensorTransformDescriptor(transformDesc));
    cudnnDestroy(cudnn);

    return 0;
}

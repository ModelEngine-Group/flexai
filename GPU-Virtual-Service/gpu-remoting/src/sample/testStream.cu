#include <assert.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <stdio.h>


int main() {

    int leastPriority, greatestPriority;
    cudaDeviceGetStreamPriorityRange(&leastPriority, &greatestPriority);
    if (leastPriority == greatestPriority) {
        printf("This device does not support stream priority.\n");
    } else {
        printf("Priority range: [%d, %d]\n", greatestPriority, leastPriority);
    }

    // Create a CUDA stream with flags
    cudaStream_t stream;
    assert(cudaSuccess == cudaStreamCreateWithFlags(&stream, cudaStreamNonBlocking));

    // Create a CUDA stream with priority
    cudaStream_t streamWithPriority;
    assert(cudaSuccess == cudaStreamCreateWithPriority(&streamWithPriority, cudaStreamNonBlocking, 1));

    // Create an event
    cudaEvent_t event;
    assert(cudaSuccess == cudaEventCreate(&event));

    printf("stream = %p, streamWithPriority = %p, event = %p\n", stream, streamWithPriority, event);

    // Record an event in the stream
    // cudaEventRecord(event, stream);

    // Wait for the event to complete in another stream
    assert(cudaSuccess == cudaStreamWaitEvent(streamWithPriority, event, 0));

    // Synchronize streams
    assert(cudaSuccess == cudaStreamSynchronize(stream));
    assert(cudaSuccess == cudaStreamSynchronize(streamWithPriority));

    // Check if a stream is capturing
    enum cudaStreamCaptureStatus capturing;
    assert(cudaSuccess == cudaStreamIsCapturing(stream, &capturing));
    if (capturing) {
        std::cout << "Stream is capturing." << std::endl;
    } else {
        std::cout << "Stream is not capturing." << std::endl;
    }

    // Get capture information
    enum cudaStreamCaptureStatus status;
    unsigned long long globalThreadId;
    assert(cudaSuccess == cudaStreamGetCaptureInfo(stream, &status, &globalThreadId));
    std::cout << "Capture Status: " << status << std::endl;
    std::cout << "Global Thread ID: " << globalThreadId << std::endl;

    // Clean up
    assert(cudaSuccess == cudaStreamDestroy(stream));
    assert(cudaSuccess == cudaStreamDestroy(streamWithPriority));
    assert(cudaSuccess == cudaEventDestroy(event));

    return 0;
}


//end
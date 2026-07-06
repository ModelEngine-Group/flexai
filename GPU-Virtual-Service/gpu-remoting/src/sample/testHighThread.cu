#include <cstdint>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <thread>
#include <vector>
#include <sys/time.h>

#define checkCUDA(expression)                                \
{                                                            \
    cudaError_t error = (expression);                        \
    if (error != cudaSuccess) {                              \
        std::cerr << "Error on line " << __LINE__ << ": "    \
                  << cudaGetErrorString(error) << std::endl; \
        std::exit(EXIT_FAILURE);                             \
    }                                                        \
}

thread_local int t_tid = 0;

inline int tid()
{
    if (t_tid == 0)
    {
        t_tid = static_cast<pid_t>(::syscall(SYS_gettid));
    }
    return t_tid;
}

// Function to be executed by each thread
void cudaTask(int priority) {
    int device;

    // if (priority == 0) {
    //     sleep(5);
    // }
   

    // Get current device
    cudaGetDevice(&device);
    // printf("Thread %d, Current CUDA device: %d\n", tid(), device);

    // Set device to 0
    checkCUDA(cudaSetDevice(0));

    // Allocate memory on device
    float* dev_A;
    size_t size = 256 * 1024 * 1024; // 256 MB
    checkCUDA(cudaMalloc((void**)&dev_A, size));
    // printf("Thread %d, Allocated %.2f MB on device %d (%p)\n", tid(), size / (1024.0 * 1024.0), device, dev_A);

    // Example host data initialization
    float* host_A = new float[size / sizeof(float)];
    for (size_t i = 0; i < size / sizeof(float); i++) {
        host_A[i] = float(i);
    }
    float* host_B = new float[8];

    // Asynchronous copy of data from host to device
    for (size_t i = 0; i < 100; i++) {
        checkCUDA(cudaMemcpyAsync(dev_A, host_A, size, cudaMemcpyHostToDevice));
        checkCUDA(cudaMemcpyAsync(host_B, dev_A, 8 * sizeof(float), cudaMemcpyDeviceToHost));
    }
    // printf("Thread %d, Copied data from host to device and back\n", tid());
    // for (size_t i = 0; i < 8; i++) {
    //     printf("%.2f%c", host_B[i], (i < 7) ? ' ' : '\n');
    // }
    checkCUDA(cudaDeviceSynchronize());

    // Clean up
    checkCUDA(cudaFree(dev_A));
    
    // Change to device 1
    checkCUDA(cudaSetDevice(1));


    delete[] host_A;
    delete[] host_B;
}

int main() {
    // Initialize CUDA device
    cudaSetDevice(1);

    const int numThreads = 16;
    std::vector<std::thread> threads;

    struct timeval timestart;
    struct timeval timeend;
    gettimeofday(&timestart, NULL);

    // Create two threads using STL
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(cudaTask, i);
    }

    // Wait for threads to finish
    for (std::thread& t : threads) {
        t.join();
    }

    // cudaTask(1);

    gettimeofday(&timeend, NULL);
    long diff = 1000000 * (timeend.tv_sec - timestart.tv_sec) +
                timeend.tv_usec - timestart.tv_usec;
    double msTotalTime = 1.0f * diff / 1000.0;
    printf("%lf\n", msTotalTime);

    // std::cout << "All threads have finished." << std::endl;

    return 0;
}


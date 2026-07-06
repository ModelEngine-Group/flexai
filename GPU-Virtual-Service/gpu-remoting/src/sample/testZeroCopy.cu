#include <assert.h>
#include <stdio.h>
#include <cuda_runtime.h>

__global__ void vectorAddGPU(float *a, float *b, float *c, int N)
{
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx < N)
    {
        c[idx] = a[idx] + b[idx];
    }
}

#define MEMORY_ALIGNMENT 4096
#define ALIGN_UP(x, size) (((size_t)x + (size - 1)) & (~(size - 1)))

int main(int argc, char **argv)
{
    int n, nelem;
    unsigned int flags;
    size_t bytes;
    float *a, *b, *c;          // Pinned memory allocated on the CPU
    float *d_a, *d_b, *d_c;    // Device pointers for mapped memory
    float errorNorm, refNorm, ref, diff;


    /* Allocate mapped CPU memory. */
    nelem = 1048576;
    bytes = nelem * sizeof(float);

    flags = cudaHostAllocMapped;
    assert(cudaSuccess == cudaHostAlloc((void **)&a, bytes, flags));
    assert(cudaSuccess == cudaHostAlloc((void **)&b, bytes, flags));
    assert(cudaSuccess == cudaHostAlloc((void **)&c, bytes, flags));
    printf("a: %p, b: %p, c: %p\n", a, b, c);

    /* Initialize the vectors. */
    for (n = 0; n < nelem; n++){
        a[n] = rand() / (float)RAND_MAX;
        b[n] = rand() / (float)RAND_MAX;
    }

    /* Get the device pointers for the pinned CPU memory mapped into the GPU
       memory space. */
    assert(cudaSuccess == cudaHostGetDevicePointer((void **)&d_a, (void *)a, 0));
    assert(cudaSuccess == cudaHostGetDevicePointer((void **)&d_b, (void *)b, 0));
    assert(cudaSuccess == cudaHostGetDevicePointer((void **)&d_c, (void *)c, 0));
    printf("d_a: %p, d_b: %p, d_c: %p\n", d_a, d_b, d_c);


    /* Call the GPU kernel using the CPU pointers residing in CPU mapped memory. */
    printf("> vectorAddGPU kernel will add vectors using mapped CPU memory...\n");
    dim3 block(256);
    dim3 grid((unsigned int)ceil(nelem / (float)block.x));
    vectorAddGPU<<<grid, block>>>(d_a, d_b, d_c, nelem);

    cudaDeviceSynchronize();

    /* Compare the results */
    printf("> Checking the results from vectorAddGPU() ...\n");
    errorNorm = 0.f;
    refNorm = 0.f;
    for (n = 0; n < nelem; n++){
        ref = a[n] + b[n];
        diff = c[n] - ref;
        errorNorm += diff * diff;
        refNorm += ref * ref;
        if (n < 10)
            printf("c[%d]=%f, ref=%f\n", n, c[n], ref);
    }

    errorNorm = (float)sqrt((double)errorNorm);
    refNorm = (float)sqrt((double)refNorm);

    

    /* Memory clean up */
    printf("> Releasing CPU memory...\n");
    assert(cudaSuccess == cudaFreeHost(a));
    assert(cudaSuccess == cudaFreeHost(b));
    assert(cudaSuccess == cudaFreeHost(c));

    

    exit(errorNorm / refNorm < 1.e-6f ? EXIT_SUCCESS : EXIT_FAILURE);
}

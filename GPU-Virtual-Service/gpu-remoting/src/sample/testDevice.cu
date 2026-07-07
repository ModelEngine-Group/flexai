#include <assert.h>
#include <cuda_runtime.h>
#include <iostream>
#include <fstream>
#include <cstring>

void compareDeviceProps(const cudaDeviceProp& prop1, const cudaDeviceProp& prop2) {

    for (size_t i = 0; i < sizeof(cudaDeviceProp); i++) {
        if (((uint8_t*)&prop1)[i] != ((uint8_t*)&prop2)[i]) {
            printf("prop1[%02lu]: %02x, prop2[%02lu]: %02x", i, ((uint8_t*)&prop1)[i], i, ((uint8_t*)&prop2)[i]);
            printf(" <- Different\n");
        }
    }

    if (prop1.major != prop2.major)
        std::cout << "Different 'major' version: " << prop1.major << " vs " << prop2.major << std::endl;
    if (prop1.minor != prop2.minor)
        std::cout << "Different 'minor' version: " << prop1.minor << " vs " << prop2.minor << std::endl;
    if (memcmp(prop1.name, prop2.name, sizeof(prop1.name)) != 0)
        std::cout << "Different 'name': " << prop1.name << " vs " << prop2.name << std::endl;
    if (prop1.totalGlobalMem != prop2.totalGlobalMem)
        std::cout << "Different 'totalGlobalMem': " << prop1.totalGlobalMem << " vs " << prop2.totalGlobalMem << std::endl;
    if (prop1.sharedMemPerBlock != prop2.sharedMemPerBlock)
        std::cout << "Different 'sharedMemPerBlock': " << prop1.sharedMemPerBlock << " vs " << prop2.sharedMemPerBlock << std::endl;
    if (prop1.regsPerBlock != prop2.regsPerBlock)
        std::cout << "Different 'regsPerBlock': " << prop1.regsPerBlock << " vs " << prop2.regsPerBlock << std::endl;
    if (prop1.warpSize != prop2.warpSize)
        std::cout << "Different 'warpSize': " << prop1.warpSize << " vs " << prop2.warpSize << std::endl;
    if (prop1.memPitch != prop2.memPitch)
        std::cout << "Different 'memPitch': " << prop1.memPitch << " vs " << prop2.memPitch << std::endl;
    if (prop1.maxThreadsPerBlock != prop2.maxThreadsPerBlock)
        std::cout << "Different 'maxThreadsPerBlock': " << prop1.maxThreadsPerBlock << " vs " << prop2.maxThreadsPerBlock << std::endl;
    if (memcmp(prop1.maxThreadsDim, prop2.maxThreadsDim, sizeof(prop1.maxThreadsDim)) != 0)
        std::cout << "Different 'maxThreadsDim': " << prop1.maxThreadsDim[0] << ", " << prop1.maxThreadsDim[1] << ", " << prop1.maxThreadsDim[2] 
                  << " vs " << prop2.maxThreadsDim[0] << ", " << prop2.maxThreadsDim[1] << ", " << prop2.maxThreadsDim[2] << std::endl;
    if (memcmp(prop1.maxGridSize, prop2.maxGridSize, sizeof(prop1.maxGridSize)) != 0)
        std::cout << "Different 'maxGridSize': " << prop1.maxGridSize[0] << ", " << prop1.maxGridSize[1] << ", " << prop1.maxGridSize[2] 
                  << " vs " << prop2.maxGridSize[0] << ", " << prop2.maxGridSize[1] << ", " << prop2.maxGridSize[2] << std::endl;
    if (prop1.clockRate != prop2.clockRate)
        std::cout << "Different 'clockRate': " << prop1.clockRate << " vs " << prop2.clockRate << std::endl;
    if (prop1.totalConstMem != prop2.totalConstMem)
        std::cout << "Different 'totalConstMem': " << prop1.totalConstMem << " vs " << prop2.totalConstMem << std::endl;
    if (prop1.multiProcessorCount != prop2.multiProcessorCount)
        std::cout << "Different 'multiProcessorCount': " << prop1.multiProcessorCount << " vs " << prop2.multiProcessorCount << std::endl;
    if (prop1.kernelExecTimeoutEnabled != prop2.kernelExecTimeoutEnabled)
        std::cout << "Different 'kernelExecTimeoutEnabled': " << prop1.kernelExecTimeoutEnabled << " vs " << prop2.kernelExecTimeoutEnabled << std::endl;
    if (prop1.integrated != prop2.integrated)
        std::cout << "Different 'integrated': " << prop1.integrated << " vs " << prop2.integrated << std::endl;
    if (prop1.canMapHostMemory != prop2.canMapHostMemory)
        std::cout << "Different 'canMapHostMemory': " << prop1.canMapHostMemory << " vs " << prop2.canMapHostMemory << std::endl;
    if (prop1.computeMode != prop2.computeMode)
        std::cout << "Different 'computeMode': " << prop1.computeMode << " vs " << prop2.computeMode << std::endl;
    if (prop1.concurrentKernels != prop2.concurrentKernels)
        std::cout << "Different 'concurrentKernels': " << prop1.concurrentKernels << " vs " << prop2.concurrentKernels << std::endl;
    if (prop1.ECCEnabled != prop2.ECCEnabled)
        std::cout << "Different 'ECCEnabled': " << prop1.ECCEnabled << " vs " << prop2.ECCEnabled << std::endl;
    if (prop1.pciBusID != prop2.pciBusID)
        std::cout << "Different 'pciBusID': " << prop1.pciBusID << " vs " << prop2.pciBusID << std::endl;
    if (prop1.pciDeviceID != prop2.pciDeviceID)
        std::cout << "Different 'pciDeviceID': " << prop1.pciDeviceID << " vs " << prop2.pciDeviceID << std::endl;
    if (prop1.pciDomainID != prop2.pciDomainID)
        std::cout << "Different 'pciDomainID': " << prop1.pciDomainID << " vs " << prop2.pciDomainID << std::endl;
    if (prop1.tccDriver != prop2.tccDriver)
        std::cout << "Different 'tccDriver': " << prop1.tccDriver << " vs " << prop2.tccDriver << std::endl;
    if (prop1.asyncEngineCount != prop2.asyncEngineCount)
        std::cout << "Different 'asyncEngineCount': " << prop1.asyncEngineCount << " vs " << prop2.asyncEngineCount << std::endl;
    if (prop1.unifiedAddressing != prop2.unifiedAddressing)
        std::cout << "Different 'unifiedAddressing': " << prop1.unifiedAddressing << " vs " << prop2.unifiedAddressing << std::endl;
    if (prop1.memoryClockRate != prop2.memoryClockRate)
        std::cout << "Different 'memoryClockRate': " << prop1.memoryClockRate << " vs " << prop2.memoryClockRate << std::endl;
    if (prop1.memoryBusWidth != prop2.memoryBusWidth)
        std::cout << "Different 'memoryBusWidth': " << prop1.memoryBusWidth << " vs " << prop2.memoryBusWidth << std::endl;
    if (prop1.l2CacheSize != prop2.l2CacheSize)
        std::cout << "Different 'l2CacheSize': " << prop1.l2CacheSize << " vs " << prop2.l2CacheSize << std::endl;
    if (prop1.maxThreadsPerMultiProcessor != prop2.maxThreadsPerMultiProcessor)
        std::cout << "Different 'maxThreadsPerMultiProcessor': " << prop1.maxThreadsPerMultiProcessor << " vs " << prop2.maxThreadsPerMultiProcessor << std::endl;
    if (prop1.streamPrioritiesSupported != prop2.streamPrioritiesSupported)
        std::cout << "Different 'streamPrioritiesSupported': " << prop1.streamPrioritiesSupported << " vs " << prop2.streamPrioritiesSupported << std::endl;
    if (prop1.globalL1CacheSupported != prop2.globalL1CacheSupported)
        std::cout << "Different 'globalL1CacheSupported': " << prop1.globalL1CacheSupported << " vs " << prop2.globalL1CacheSupported << std::endl;
    if (prop1.localL1CacheSupported != prop2.localL1CacheSupported)
        std::cout << "Different 'localL1CacheSupported': " << prop1.localL1CacheSupported << " vs " << prop2.localL1CacheSupported << std::endl;
    if (prop1.sharedMemPerMultiprocessor != prop2.sharedMemPerMultiprocessor)
        std::cout << "Different 'sharedMemPerMultiprocessor': " << prop1.sharedMemPerMultiprocessor << " vs " << prop2.sharedMemPerMultiprocessor << std::endl;
    if (prop1.reservedSharedMemPerBlock != prop2.reservedSharedMemPerBlock)
        std::cout << "Different 'reservedSharedMemPerBlock': " << prop1.reservedSharedMemPerBlock << " vs " << prop2.reservedSharedMemPerBlock << std::endl;
    if (prop1.hostNativeAtomicSupported != prop2.hostNativeAtomicSupported)
        std::cout << "Different 'hostNativeAtomicSupported': " << prop1.hostNativeAtomicSupported << " vs " << prop2.hostNativeAtomicSupported << std::endl;
    if (prop1.singleToDoublePrecisionPerfRatio != prop2.singleToDoublePrecisionPerfRatio)
        std::cout << "Different 'singleToDoublePrecisionPerfRatio': " << prop1.singleToDoublePrecisionPerfRatio << " vs " << prop2.singleToDoublePrecisionPerfRatio << std::endl;
    if (prop1.pageableMemoryAccess != prop2.pageableMemoryAccess)
        std::cout << "Different 'pageableMemoryAccess': " << prop1.pageableMemoryAccess << " vs " << prop2.pageableMemoryAccess << std::endl;
    if (prop1.pageableMemoryAccessUsesHostPageTables != prop2.pageableMemoryAccessUsesHostPageTables)
        std::cout << "Different 'pageableMemoryAccessUsesHostPageTables': " << prop1.pageableMemoryAccessUsesHostPageTables << " vs " << prop2.pageableMemoryAccessUsesHostPageTables << std::endl;
    if (prop1.directManagedMemAccessFromHost != prop2.directManagedMemAccessFromHost)
        std::cout << "Different 'directManagedMemAccessFromHost': " << prop1.directManagedMemAccessFromHost << " vs " << prop2.directManagedMemAccessFromHost << std::endl;
}

int main(int argc, char* argv[]) {
    int gpuid = 0;
    cudaGetDevice(&gpuid);
    std::cout << "gpuid: " << gpuid << std::endl;

    int cnt = 0;
    cudaGetDeviceCount(&cnt);
    std::cout<<"cnt: "<<cnt<<std::endl;
    int devID = 0;
    cudaSetDevice(devID);
    cudaDeviceProp deviceProp;
    cudaGetDeviceProperties(&deviceProp, devID);
    printf("prop size: %zu\n", sizeof(deviceProp));
    printf("prop->name: %s\n", deviceProp.name);
    printf("prop->totalGlobalMem: %lu\n", deviceProp.totalGlobalMem);
    //printf("prop->sharedMemPerBlock: %lu\n", prop->sharedMemPerBlock);
    //printf("prop->regsPerBlock: %d\n", prop->regsPerBlock);
    printf("prop->warpSize: %d\n", deviceProp.warpSize);
    //printf("prop->memPitch: %lu\n", prop->memPitch);
    printf("prop->maxThreadsPerBlock: %d\n", deviceProp.maxThreadsPerBlock);
    printf("prop->maxThreadsPerMultiProcessor: %d\n", deviceProp.maxThreadsPerMultiProcessor);


    // // 打开文件并读取内容
    // std::ifstream inFile("monitor-2.bin", std::ios::in | std::ios::binary);
    // if (!inFile) {
    //     std::cerr << "Failed to open file for reading." << std::endl;
    //     return 1;
    // }
    // else {
    //     std::cout << "File opened: monitor-2.bin" << std::endl;
    // }

    // cudaDeviceProp fileDeviceProp;
    // inFile.read(reinterpret_cast<char*>(&fileDeviceProp), sizeof(cudaDeviceProp));

    // // 关闭文件
    // inFile.close();

    // std::cout << "=====================" << std::endl;
    // // 比较从文件读取的cudaDeviceProp和从CUDA API获取的cudaDeviceProp
    // compareDeviceProps(deviceProp, fileDeviceProp);
    // std::cout << "=====================" << std::endl;


    int val;
    cudaDeviceGetAttribute(&val, cudaDevAttrMaxThreadsPerBlock, devID);
    std::cout<<"maxThreadsPerBlock: "<<val<<std::endl;
    cudaDeviceGetAttribute(&val, cudaDevAttrWarpSize, devID);
    std::cout<<"WarpSize: "<<val<<std::endl;


    int device_count = 0;
    cudaGetDeviceCount(&device_count);
    if (device_count == 0) {
        printf("There is no device supporting CUDA\n");
        exit(EXIT_FAILURE);
    }
    struct cudaDeviceProp *devicePropList = new struct cudaDeviceProp[device_count];
    for (int i = 0; i < device_count; i++) {
        cudaGetDeviceProperties(&devicePropList[i], i);
        std::cout << "Device " << i << ": " << devicePropList[i].name << std::endl;
        std::cout << "Total global memory: " << devicePropList[i].totalGlobalMem << std::endl;
        std::cout << "Shared memory per block: " << devicePropList[i].sharedMemPerBlock << std::endl;
    }
}

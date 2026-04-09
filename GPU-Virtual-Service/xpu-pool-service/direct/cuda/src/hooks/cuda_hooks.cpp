#include <atomic>
#include <cuda.h>
#include "cuda_resource_limiter.h"
#include "hook_helper.h"
#include "file_lock.h"
#include "log.h"

static std::unordered_map<void *, void*> g_hookedProc = {
  PROC_ADDR_PAIR(cuDriverGetVersion),
  PROC_ADDR_PAIR(cuInit),
  PROC_ADDR_PAIR(cuGetProcAddress),
  PROC_ADDR_PAIR(cuGetProcAddress_v2),
  PROC_ADDR_PAIR(cuCtxCreate),
  PROC_ADDR_PAIR(cuCtxCreate_v2),
  PROC_ADDR_PAIR(cuCtxCreate_v3),
  PROC_ADDR_PAIR(cuCtxPushCurrent),
  PROC_ADDR_PAIR(cuCtxPushCurrent_v2),
  PROC_ADDR_PAIR(cuCtxPopCurrent),
  PROC_ADDR_PAIR(cuCtxPopCurrent_v2),
  PROC_ADDR_PAIR(cuCtxSetCurrent),
  PROC_ADDR_PAIR(cuMemAllocManaged),
  PROC_ADDR_PAIR(cuMemAlloc_v2),
  PROC_ADDR_PAIR(cuMemAlloc),
  PROC_ADDR_PAIR(cuMemAllocPitch_v2),
  PROC_ADDR_PAIR(cuMemAllocPitch),
  PROC_ADDR_PAIR(cuArrayCreate_v2),
  PROC_ADDR_PAIR(cuArrayCreate),
  PROC_ADDR_PAIR(cuArray3DCreate_v2),
  PROC_ADDR_PAIR(cuArray3DCreate),
  PROC_ADDR_PAIR(cuMipmappedArrayCreate),
  PROC_ADDR_PAIR(cuDeviceTotalMem_v2),
  PROC_ADDR_PAIR(cuDeviceTotalMem),
  PROC_ADDR_PAIR(cuMemGetInfo_v2),
  PROC_ADDR_PAIR(cuMemGetInfo),
  PROC_ADDR_PAIR(cuLaunchKernel_ptsz),
  PROC_ADDR_PAIR(cuLaunchKernel),
  PROC_ADDR_PAIR(cuLaunchKernelEx),
  PROC_ADDR_PAIR(cuLaunchKernelEx_ptsz),
  PROC_ADDR_PAIR(cuLaunch),
  PROC_ADDR_PAIR(cuLaunchCooperativeKernel_ptsz),
  PROC_ADDR_PAIR(cuLaunchCooperativeKernel),
  PROC_ADDR_PAIR(cuLaunchCooperativeKernelMultiDevice),
  PROC_ADDR_PAIR(cuLaunchGrid),
  PROC_ADDR_PAIR(cuLaunchGridAsync),
  PROC_ADDR_PAIR(cuGraphLaunch),
  PROC_ADDR_PAIR(cuModuleGetFunction),
};

template <typename Integer1, typename Integer2>
inline static constexpr Integer1 RoundUp(Integer1 n, Integer2 base)
{
  return (n % base) ? (n + base - (n % base)) : n;
}

inline static size_t CuarrayElementSize(int format) {
  switch (format) {
    case CU_AD_FORMAT_UNSIGNED_INT8:
    case CU_AD_FORMAT_SIGNED_INT8:
      return sizeof(int8_t);
    case CU_AD_FORMAT_UNSIGNED_INT16:
    case CU_AD_FORMAT_SIGNED_INT16:
    case CU_AD_FORMAT_HALF:
      return sizeof(int16_t);
    case CU_AD_FORMAT_UNSIGNED_INT32:
    case CU_AD_FORMAT_SIGNED_INT32:
    case CU_AD_FORMAT_FLOAT:
      return sizeof(int32_t);
    default:
      return sizeof(int64_t);
  }
}

template <typename Descriptor>
inline static size_t CalCuarraySize(const Descriptor *pAllocateArray) {
  size_t elementSize = CuarrayElementSize(pAllocateArray->Format);
  size_t height = (pAllocateArray->Height == 0) ? 1 : pAllocateArray->Height;
  return elementSize * pAllocateArray->NumChannels * pAllocateArray->Width * height;
}

template <typename Descriptor>
inline static size_t CalCuarray3dSize(const Descriptor *pAllocateArray) {
  size_t elementSize = CuarrayElementSize(pAllocateArray->Format);
  size_t height = (pAllocateArray->Height == 0) ? 1 : pAllocateArray->Height;
  size_t depth = (pAllocateArray->Depth == 0) ? 1 : pAllocateArray->Depth;
  return elementSize * pAllocateArray->NumChannels * pAllocateArray->Width * height * depth;
}

extern "C" {
CUresult FUNC_HOOK_BEGIN(cuDriverGetVersion, int *driverVersion)
  CudaResourceLimiter::Instance().Initialize();
  return original(driverVersion);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuInit, unsigned int flags)
  CudaResourceLimiter::Instance().Initialize();
  return original(flags);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuGetProcAddress, const char *symbol, void **pfn, int cudaVersion,
                           cuuint64_t flags)
  CudaResourceLimiter::Instance().Initialize();
  void *fnPtr = nullptr;
  CUresult ret = original(symbol, &fnPtr, cudaVersion, flags);
  auto pair = g_hookedProc.find(fnPtr);
  if (pair != g_hookedProc.end()) {
    *pfn = pair->second;
  } else {
    *pfn = fnPtr;
  }

  return ret;
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuGetProcAddress_v2, const char *symbol, void **pfn, int cudaVersion,
                           cuuint64_t flags, CUdriverProcAddressQueryResult *result)
  CudaResourceLimiter::Instance().Initialize();
  void *fnPtr = nullptr;
  CUresult ret = original(symbol, &fnPtr, cudaVersion, flags, result);
  auto pair = g_hookedProc.find(fnPtr);
  if (pair != g_hookedProc.end()) {
    *pfn = pair->second;
  } else {
    *pfn = fnPtr;
  }
  return ret;
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemAllocManaged, CUdeviceptr *dptr, size_t bytesize, unsigned int flags)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(bytesize);
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(dptr, bytesize, flags);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemAlloc_v2, CUdeviceptr *dptr, size_t bytesize)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(bytesize);
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(dptr, bytesize);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemAlloc, CUdeviceptr_v1 *dptr, unsigned int bytesize)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(bytesize);
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(dptr, bytesize);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemAllocPitch_v2, CUdeviceptr *dptr, size_t *pPitch, size_t WidthInBytes,
                         size_t Height, unsigned int ElementSizeBytes)
  size_t bytesize = RoundUp(WidthInBytes * Height, ElementSizeBytes);
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(bytesize);
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(dptr, pPitch, WidthInBytes, Height, ElementSizeBytes);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemAllocPitch, CUdeviceptr_v1 *dptr, unsigned int *pPitch, unsigned int WidthInBytes,
                         unsigned int Height, unsigned int ElementSizeBytes)
  unsigned int bytesize = RoundUp(WidthInBytes * Height, ElementSizeBytes);
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(bytesize);
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(dptr, pPitch, WidthInBytes, Height, ElementSizeBytes);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuArrayCreate_v2, CUarray *pHandle, const CUDA_ARRAY_DESCRIPTOR *pAllocateArray)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(CalCuarraySize(pAllocateArray));
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(pHandle, pAllocateArray);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuArrayCreate, CUarray *pHandle, const CUDA_ARRAY_DESCRIPTOR_v1 *pAllocateArray)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(CalCuarraySize(pAllocateArray));
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(pHandle, pAllocateArray);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuArray3DCreate_v2, CUarray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR *pAllocateArray)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(CalCuarray3dSize(pAllocateArray));
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(pHandle, pAllocateArray);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuArray3DCreate, CUarray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR_v1 *pAllocateArray)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(CalCuarray3dSize(pAllocateArray));
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(pHandle, pAllocateArray);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMipmappedArrayCreate, CUmipmappedArray *pHandle, const CUDA_ARRAY3D_DESCRIPTOR *pMipmappedArrayDesc,
    unsigned int numMipmapLevels)
  auto memGuard = CudaResourceLimiter::Instance().GuardedMemoryCheck(CalCuarray3dSize(pMipmappedArrayDesc));
  if (memGuard.Error()) {
    return CUDA_ERROR_UNKNOWN;
  }
  if (!memGuard.enough) {
    return CUDA_ERROR_OUT_OF_MEMORY;
  }
  return original(pHandle, pMipmappedArrayDesc, numMipmapLevels);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuDeviceTotalMem_v2, size_t *bytes, CUdevice dev)
  if (CudaResourceLimiter::Instance().LimitMemory()) {
    *bytes = CudaResourceLimiter::Instance().MemoryQuota();
    return CUDA_SUCCESS;
  }
  return original(bytes, dev);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuDeviceTotalMem, unsigned int *bytes, CUdevice dev)
  if (CudaResourceLimiter::Instance().LimitMemory()) {
    *bytes = CudaResourceLimiter::Instance().MemoryQuota();
    return CUDA_SUCCESS;
  }
  return original(bytes, dev);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemGetInfo_v2, size_t *free, size_t *total)
  if (CudaResourceLimiter::Instance().LimitMemory()) {
    size_t used;
    int ret = CudaResourceLimiter::Instance().MemoryUsed(used);
    if (ret) {
      return CUDA_ERROR_NOT_FOUND;
    }
    *total = CudaResourceLimiter::Instance().MemoryQuota();
    *free = used >= *total ? 0 : *total - used;
    return CUDA_SUCCESS;
  }
  return original(free, total);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuMemGetInfo, unsigned int *free, unsigned int *total)
  if (CudaResourceLimiter::Instance().LimitMemory()) {
    size_t used;
    int ret = CudaResourceLimiter::Instance().MemoryUsed(used);
    if (ret) {
      return CUDA_ERROR_NOT_FOUND;
    }
    *total = CudaResourceLimiter::Instance().MemoryQuota();
    *free = used >= *total ? 0 : *total - used;
    return CUDA_SUCCESS;
  }
  return original(free, total);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuModuleGetFunction, CUfunction *hfunc, CUmodule hmod, const char *name)
  CUresult ret = original(hfunc, hmod, name);
  return ret;
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchKernel_ptsz, CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes,
    CUstream hStream, void **kernelParams, void **extra)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams, extra);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchKernel, CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes,
    CUstream hStream, void **kernelParams, void **extra)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams, extra);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchKernelEx, const CUlaunchConfig *launchConfig, CUfunction f, void **kernelParams, void **extra)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(launchConfig, f, kernelParams, extra);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchKernelEx_ptsz, const CUlaunchConfig *launchConfig, CUfunction f, void **kernelParams, void **extra)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(launchConfig, f, kernelParams, extra);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunch, CUfunction f)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchCooperativeKernel_ptsz, CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes,
    CUstream hStream, void **kernelParams)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchCooperativeKernel, CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
    unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes,
    CUstream hStream, void **kernelParams)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchCooperativeKernelMultiDevice, CUDA_LAUNCH_PARAMS *launchParamsList, unsigned int numDevices,
    unsigned int flags)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(launchParamsList, numDevices, flags);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchGrid, CUfunction f, int grid_width, int grid_height)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f, grid_width, grid_height);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuLaunchGridAsync, CUfunction f, int grid_width, int grid_height, CUstream hStream)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(f, grid_width, grid_height, hStream);
FUNC_HOOK_END

CUresult FUNC_HOOK_BEGIN(cuGraphLaunch, CUgraphExec hGraphExec, CUstream hStream)
  CudaResourceLimiter::Instance().ComputingPowerLimiter();
  return original(hGraphExec, hStream);
FUNC_HOOK_END
}

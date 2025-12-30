/*
 * Copyright (c) Huawei Technologies Co., Ltd. 2024-2025. All rights reserved.
 */

 #include <atomic>
 #include <acl/acl.h>
 #include <runtime/rt.h>
 #include "acl_resource_limiter.h"
 #include "hook_helper.h"
 #include "log.h"
 
 extern "C" {
 // 以下函数在头文件中存在，但在libruntime.so中找不到对应的符号；
 // rtInit
 // rtVectorCoreKernelLaunchWithHandle
 // rtVectorCoreKernelLaunch
 // 这些函数为尽可能保证兼容性仍继续hook，但是不包含在UT测试中
 
 // libruntime.so
 rtError_t FUNC_HOOK_BEGIN(rtSetDevice, int32_t devId)
     AclResourceLimiter::Instance().Initialize();
     return original(devId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtSetTSDevice, uint32_t tsId)
     AclResourceLimiter::Instance().Initialize();
     return original(tsId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtInit)
     AclResourceLimiter::Instance().Initialize();
     return original();
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtDvppMalloc, void **devPtr, uint64_t size, const uint16_t moduleId)
     auto memGuard = AclResourceLimiter::Instance().GuardedMemoryCheck(size);
     if (memGuard.Error()) {
         return ACL_ERROR_FAILURE;
     }
     if (!memGuard.enough) {
         return ACL_ERROR_STORAGE_OVER_LIMIT;
     }
     return original(devPtr, size, moduleId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtMalloc, void **devPtr, uint64_t size, rtMemType_t type, const uint16_t moduleId)
     auto memGuard = AclResourceLimiter::Instance().GuardedMemoryCheck(size);
     if (memGuard.Error()) {
         return ACL_ERROR_FAILURE;
     }
     if (!memGuard.enough) {
         return ACL_ERROR_STORAGE_OVER_LIMIT;
     }
     return original(devPtr, size, type, moduleId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtDvppMallocWithFlag, void **devPtr, uint64_t size, uint32_t flag, const uint16_t moduleId)
     auto memGuard = AclResourceLimiter::Instance().GuardedMemoryCheck(size);
     if (memGuard.Error()) {
         return ACL_ERROR_FAILURE;
     }
     if (!memGuard.enough) {
         return ACL_ERROR_STORAGE_OVER_LIMIT;
     }
     return original(devPtr, size, flag, moduleId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtMallocManaged, void **ptr, uint64_t size, uint32_t flag, const uint16_t moduleId)
     auto memGuard = AclResourceLimiter::Instance().GuardedMemoryCheck(size);
     if (memGuard.Error()) {
         return ACL_ERROR_FAILURE;
     }
     if (!memGuard.enough) {
         return ACL_ERROR_STORAGE_OVER_LIMIT;
     }
     return original(ptr, size, flag, moduleId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtMallocCached, void **devPtr, uint64_t size, rtMemType_t type, const uint16_t moduleId)
     auto memGuard = AclResourceLimiter::Instance().GuardedMemoryCheck(size);
     if (memGuard.Error()) {
         return ACL_ERROR_FAILURE;
     }
     if (!memGuard.enough) {
         return ACL_ERROR_STORAGE_OVER_LIMIT;
     }
     return original(devPtr, size, type, moduleId);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtMallocPhysical, rtDrvMemHandle_t** handle, size_t size, rtDrvMemProp_t* prop, 
                           uint64_t flags)
     auto memGuard = AclResourceLimiter::Instance().GuardedMemoryCheck(size);
     if (memGuard.Error()) {
         return ACL_ERROR_FAILURE;
     }
     if (!memGuard.enough) {
         return ACL_ERROR_STORAGE_OVER_LIMIT;
     }
     return original(handle, size, prop, flags);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtKernelLaunch, const void *stubFunc, uint32_t blockDim, void *args, uint32_t argsSize, 
                           rtSmDesc_t *smDesc, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(stubFunc, blockDim, args, argsSize, smDesc, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtKernelLaunchWithHandle, void *hdl, const uint64_t tilingKey, uint32_t blockDim, 
                           rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, 
                           const void *kernelInfo)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(hdl, tilingKey, blockDim, argsInfo, smDesc, stm, kernelInfo);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtKernelLaunchWithHandleV2, void *hdl, const uint64_t tilingKey, uint32_t blockDim, 
                           rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, 
                           const rtTaskCfgInfo_t *cfgInfo)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(hdl, tilingKey, blockDim, argsInfo, smDesc, stm, cfgInfo);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtKernelLaunchWithFlag, const void *stubFunc, uint32_t blockDim, rtArgsEx_t *argsInfo, 
                           rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(stubFunc, blockDim, argsInfo, smDesc, stm, flags);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtKernelLaunchEx, void *args, uint32_t argsSize, uint32_t flags, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(args, argsSize, flags, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtKernelLaunchFwk, const char_t *opName, void *args, uint32_t argsSize, uint32_t flags, 
                           rtStream_t rtStream)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(rtStream);
     return original(opName, args, argsSize, flags, rtStream);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtCpuKernelLaunch, const void *soName, const void *kernelName, uint32_t blockDim, 
                           const void *args, uint32_t argsSize, rtSmDesc_t *smDesc, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(soName, kernelName, blockDim, args, argsSize, smDesc, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtAicpuKernelLaunch, const rtKernelLaunchNames_t *launchNames, uint32_t blockDim, 
                           const void *args, uint32_t argsSize, rtSmDesc_t *smDesc, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(launchNames, blockDim, args, argsSize, smDesc, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtCpuKernelLaunchWithFlag, const void *soName, const void *kernelName, uint32_t blockDim, 
                           const rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(soName, kernelName, blockDim, argsInfo, smDesc, stm, flags);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtAicpuKernelLaunchWithFlag, const rtKernelLaunchNames_t *launchNames, uint32_t blockDim, 
                           const rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(launchNames, blockDim, argsInfo, smDesc, stm, flags);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtAicpuKernelLaunchExWithArgs, const uint32_t kernelType, const char_t * const opName, 
                           const uint32_t blockDim, const rtAicpuArgsEx_t *argsInfo, rtSmDesc_t * const smDesc, 
                           const rtStream_t stm, const uint32_t flags)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(kernelType, opName, blockDim, argsInfo, smDesc, stm, flags);
 FUNC_HOOK_END
 
 // rtLaunch 间接调用 rtKernelLaunch 来执行，因此不需要 hook
 rtError_t FUNC_HOOK_BEGIN(rtLaunchKernelByFuncHandle, rtFuncHandle funcHandle, uint32_t blockDim, 
                           rtLaunchArgsHandle argsHandle, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(funcHandle, blockDim, argsHandle, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtLaunchKernelByFuncHandleV2, rtFuncHandle funcHandle, uint32_t blockDim, 
                           rtLaunchArgsHandle argsHandle, rtStream_t stm, const rtTaskCfgInfo_t *cfgInfo)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(funcHandle, blockDim, argsHandle, stm, cfgInfo);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtLaunchKernelByFuncHandleV3, rtFuncHandle funcHandle, uint32_t blockDim, 
                           const rtArgsEx_t *argsInfo, rtStream_t stm, const rtTaskCfgInfo_t * const cfgInfo)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(funcHandle, blockDim, argsInfo, stm, cfgInfo);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtVectorCoreKernelLaunchWithHandle, void *hdl, const uint64_t tilingKey, 
                           uint32_t blockDim, rtArgsEx_t *argsInfo, rtSmDesc_t *smDesc, rtStream_t stm, 
                           const rtTaskCfgInfo_t *cfgInfo)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(hdl, tilingKey, blockDim, argsInfo, smDesc, stm, cfgInfo);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtVectorCoreKernelLaunch, const void *stubFunc, uint32_t blockDim, rtArgsEx_t *argsInfo, 
                           rtSmDesc_t *smDesc, rtStream_t stm, uint32_t flags, const rtTaskCfgInfo_t *cfgInfo)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(stubFunc, blockDim, argsInfo, smDesc, stm, flags, cfgInfo);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtFftsPlusTaskLaunch, rtFftsPlusTaskInfo_t *fftsPlusTaskInfo, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(fftsPlusTaskInfo, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtFftsPlusTaskLaunchWithFlag, rtFftsPlusTaskInfo_t *fftsPlusTaskInfo, rtStream_t stm, 
                           uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(fftsPlusTaskInfo, stm, flag);
 FUNC_HOOK_END
 
 // rtNanoModelExecute 没有stream参数，也没有参考实现，无法确定会在哪个stream上执行，所以不hook
 rtError_t FUNC_HOOK_BEGIN(rtModelExecute, rtModel_t mdl, rtStream_t stm, uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(mdl, stm, flag);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtStarsTaskLaunch, const void *taskSqe, uint32_t sqelen, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(taskSqe, sqelen, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtStarsTaskLaunchWithFlag, const void *taskSqe, uint32_t sqelen, rtStream_t stm, 
                           uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(taskSqe, sqelen, stm, flag);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtCmoTaskLaunch, rtCmoTaskInfo_t *taskInfo, rtStream_t stm, uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(taskInfo, stm, flag);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtCmoAddrTaskLaunch, void *cmoAddrInfo, uint64_t destMax, rtCmoOpCode_t cmoOpCode, 
                           rtStream_t stm, uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(cmoAddrInfo, destMax, cmoOpCode, stm, flag);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtBarrierTaskLaunch, rtBarrierTaskInfo_t *taskInfo, rtStream_t stm, uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(taskInfo, stm, flag);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtMultipleTaskInfoLaunch, const void *taskInfo, rtStream_t stm)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(taskInfo, stm);
 FUNC_HOOK_END
 
 rtError_t FUNC_HOOK_BEGIN(rtMultipleTaskInfoLaunchWithFlag, const void *taskInfo, rtStream_t stm, 
                           const uint32_t flag)
     auto ret = AclResourceLimiter::Instance().ComputingPowerLimiter(stm);
     return original(taskInfo, stm, flag);
 FUNC_HOOK_END
 }
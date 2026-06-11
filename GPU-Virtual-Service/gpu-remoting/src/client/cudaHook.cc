#include "../../include/hook/hook.h"
#include <dlfcn.h>

void* get_cuda_handle() {
    static void* handle = nullptr;
    if (!handle) {
        handle = dlopen("/usr/lib/x86_64-linux-gnu/libcuda.so", RTLD_NOW | RTLD_LOCAL);
        if (!handle) {
            tool::Logging(LOG_ERROR, HOOK_LOG_TAG, "Failed to load libcuda.so: \n", dlerror());
        }
    }
    return handle;
}

CUresult cuInit(unsigned int Flags) {
    const char* func_name = "cuInit";
    // HookLog(func_name);
    using func_ptr = CUresult (*)(unsigned int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(Flags);
}

CUresult cuDeviceGetCount(int *count) {
    const char* func_name = "cuDeviceGetCount";
    // HookLog(func_name);
    using func_ptr = CUresult (*)(int * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(count);
}

CUresult cuModuleLoadData(CUmodule *module, const void *image) {
    const char* func_name = "cuModuleLoadData";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUmodule * , const void * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(module, image);
}

CUresult cuModuleGetFunction(CUfunction *hfunc, CUmodule hmod, const char *name) {
    const char* func_name = "cuModuleGetFunction";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUfunction * , CUmodule, const char * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(hfunc, hmod, name);
}

CUresult cuOccupancyMaxActiveBlocksPerMultiprocessor(int *numBlocks, CUfunction func, int blockSize, size_t dynamicSMemSize) {
    const char* func_name = "cuOccupancyMaxActiveBlocksPerMultiprocessor";
    HookLog(func_name);
    using func_ptr = CUresult (*)(int * , CUfunction, int, size_t);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(numBlocks, func, blockSize, dynamicSMemSize);
}

CUresult cuGetErrorString(CUresult error, const char **pStr) {
    const char* func_name = "cuGetErrorString";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUresult, const char **);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    *pStr = "CUDA_SUCCESS";
    return CUDA_SUCCESS;
    // return func_entry(error, pStr);
}

CUresult cuCtxGetCurrent(CUcontext *pctx) {
    const char* func_name = "cuCtxGetCurrent";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUcontext * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(pctx);
}

CUresult cuModuleUnload(CUmodule hmod) {
    const char* func_name = "cuModuleUnload";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUmodule);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(hmod);
}

CUresult cuDevicePrimaryCtxGetState(CUdevice dev, unsigned int *flags, int *active) {
    const char* func_name = "cuDevicePrimaryCtxGetState";
    HookLog(func_name, false);
    using func_ptr = CUresult (*)(CUdevice, unsigned int * , int * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    *flags = 0;
    *active = 1;
    tool::Logging(LOG_DEBUG, func_name, "dev: %d\n", dev);
    return CUDA_SUCCESS;
    // return func_entry(dev, flags, active);
}

CUresult cuLinkCreate(unsigned int numOptions, CUjit_option *options, void **optionValues, CUlinkState *stateOut) {
    const char* func_name = "cuLinkCreate";
    HookLog(func_name);
    using func_ptr = CUresult (*)(unsigned int, CUjit_option * , void * * , CUlinkState * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(numOptions, options, optionValues, stateOut);
}

CUresult cuLinkComplete(CUlinkState state, void **cubinOut, size_t *sizeOut) {
    const char* func_name = "cuLinkComplete";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUlinkState, void * * , size_t * );
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(state, cubinOut, sizeOut);
}

CUresult cuFuncSetAttribute(CUfunction hfunc, CUfunction_attribute attrib, int value) {
    const char* func_name = "cuFuncSetAttribute";
    HookLog(func_name);
    using func_ptr = CUresult (*)(CUfunction, CUfunction_attribute, int);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(hfunc, attrib, value);
}

CUresult cuFuncGetAttribute(int *pi, CUfunction_attribute attrib, CUfunction hfunc) {
    const char* func_name = "cuFuncGetAttribute";
    HookLog(func_name);
    using func_ptr = CUresult (*)(int * , CUfunction_attribute, CUfunction);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_cuda_handle(), func_name));
    return func_entry(pi, attrib, hfunc);
}

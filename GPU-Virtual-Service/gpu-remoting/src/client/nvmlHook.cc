#include "../../include/hook/hook.h"

void* get_nvml_handle() {
    static void* handle = nullptr;
    if (!handle) {
        handle = dlopen("libnvidia-ml.so", RTLD_LAZY);
        if (!handle) {
            tool::Logging(LOG_ERROR, HOOK_LOG_TAG, "Failed to load llibnvidia-ml.so: \n", dlerror());
        }
    }
    return handle;
}

nvmlReturn_t nvmlInit_v2(void) {
    const char* func_name = "nvmlInit_v2";
    HookLog(func_name, false);
    using func_ptr = nvmlReturn_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nvml_handle(), func_name));

    return NVML_SUCCESS;

    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    // reqBuf.PushRequestType(NVML_INIT_V2);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    // return func_entry();
}

nvmlReturn_t nvmlShutdown(void) {
    const char* func_name = "nvmlShutdown";
    HookLog(func_name, false);
    using func_ptr = nvmlReturn_t (*)();
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nvml_handle(), func_name));

    // printf("zwx: nvmlDeviceGetCount_v2\n");

    return NVML_SUCCESS;

    // RequestBuffer reqBuf = RequestBuffer(sizeof(int));
    // reqBuf.PushRequestType(NVML_SHUTDOWN);
    // clientEpObj->AddIOV(reqBuf.GetSize(), reqBuf._dataBuffer);
    // clientEpObj->SendRequest();
    // return func_entry();
}

nvmlReturn_t nvmlDeviceGetCount_v2(unsigned int* deviceCount) {
    std::call_once(initFlag, Intialize); // for nvidia-smi hook
    const char* func_name = "nvmlDeviceGetCount_v2";
    HookLog(func_name, false);
    using func_ptr = nvmlReturn_t (*)(unsigned int*);
    auto func_entry = reinterpret_cast<func_ptr>(dlsym(get_nvml_handle(), func_name));

    // RequestIOV reqBuf = RequestIOV();
    // reqBuf.PushRequestType(CUDA_GET_DEVICE_COUNT);
    // int tmpDev = 0;
    // reqBuf.Push(tmpDev);

    // RequestIOV resBuf = RequestIOV();
    // resBuf.Push(deviceCount);
    // clientEpObj->SendRequestRecvResponse(&reqBuf, &resBuf);
    *deviceCount = config_->GetReqGPUnum();


    return NVML_SUCCESS;
}

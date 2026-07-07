#include "../../include/serverEndpoint.h"

static const char* myName = "CUDARuntimeDeviceHandle";

DEFINE_SERVER_AM_CALLBACK(cudaGetDeviceHandle) {
    tool::Logging(myName, "CUDA_GET_DEVICE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             device      = 0;

    cudaError_t     exit_code   = cudaGetDevice(&device);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaGetDevice success, device = %d\n", device);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_GET_DEVICE);
        resBuf.Push(device);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaGetDevice failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaGetDeviceCountHandle) {
    tool::Logging(myName, "CUDA_GET_DEVICE_COUNT\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             count       = 0;

    cudaError_t     exit_code   = cudaGetDeviceCount(&count);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaGetDeviceCount success, count = %d\n", count);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_GET_DEVICE_COUNT);
        resBuf.Push(count);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaGetDeviceCount failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaGetDevicePropertiesHandle) {
    tool::Logging(myName, "CUDA_GET_DEVICE_PROPERTIES\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             device      = reqBuf.Pop<int>();
    cudaDeviceProp  prop;

    cudaError_t     exit_code   = cudaGetDeviceProperties(&prop, device);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaGetDeviceProperties success, device = %d\n", device);
        prop.canMapHostMemory = 0; //! disable UVA
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_GET_DEVICE_PROPERTIES);
        resBuf.Push(prop);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaGetDeviceProperties failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaDeviceGetAttributeHandle) {
    tool::Logging(myName, "CUDA_DEVICE_GET_ATTRIBUTE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             value       = 0;
    cudaDeviceAttr  attr        = (cudaDeviceAttr)reqBuf.Pop<int>();
    int             device      = reqBuf.Pop<int>();

    cudaError_t     exit_code   = cudaDeviceGetAttribute(&value, attr, device);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaDeviceGetAttribute success, device = %d, attr = %d, value = %d\n", device, attr, value);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_DEVICE_GET_ATTRIBUTE);
        resBuf.Push(value);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaDeviceGetAttribute failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaSetDeviceHandle) {
    tool::Logging(myName, "CUDA_SET_DEVICE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             deviceIdx   = reqBuf.Pop<int>(); //! index of devList, not device id

    cudaError_t     exit_code   = cudaSetDevice(deviceIdx);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaSetDevice success, device = %d\n", deviceIdx);
        serverEp->curDev_ = deviceIdx;
        serverEp->CliDev_ = deviceIdx;
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaSetDevice failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaSetMainDeviceHandle) {
    tool::Logging(myName, "CUDA_SET_MAIN_DEVICE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    int             deviceIdx   = reqBuf.Pop<int>();
    serverEp->clientPID_        = reqBuf.Pop<int>();
    serverEp->priority_         = reqBuf.Pop<size_t>();
    serverEp->recoveryFlag_     = reqBuf.Pop<bool>();

    serverEp->replayFlag_ = serverEp->recoveryFlag_;
    cudaError_t     exit_code   = cudaSetDevice(deviceIdx);
    if (exit_code == cudaSuccess) {
        tool::Logging(LOG_INFO, serverEp->myName_, "cudaSetMainDevice success, curDev_ = %d, clientPID_ = %d, priority_ = %zu\n", deviceIdx, serverEp->clientPID_, serverEp->priority_);
        serverEp->curDev_ = deviceIdx;
        serverEp->CliDev_ = deviceIdx;
        serverEp->backupFilePath_ = std::string(BACKUP_FILE_DIR) + "flexgv_backup_" + std::to_string(serverEp->clientID_) + "_" + std::to_string(serverEp->clientPID_) + ".dat";

        // serverEp->SendStatus(123);

        if (serverEp->recoveryFlag_) {
            serverEp->LoadFromStorage();
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaSetMainDevice failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaDeviceSynchronizeHandle) {
    tool::Logging(myName, "CUDA_DEVICE_SYNCHRONIZE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaError_t     exit_code   = cudaDeviceSynchronize();
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaDeviceSynchronize success\n");
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaDeviceSynchronize failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}
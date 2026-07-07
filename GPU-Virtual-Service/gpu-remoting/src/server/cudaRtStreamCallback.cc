#include "../../include/serverEndpoint.h"

static const char* myName = "CUDARuntimeStreamHandle";

DEFINE_SERVER_AM_CALLBACK(cudaStreamCreateHandle) {
    tool::Logging(myName, "CUDA_STREAM_CREATE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        virAddr     = reqBuf.Pop<uint64_t>();
    cudaStream_t    stream;
    cudaError_t     exit_code   = cudaStreamCreate(&stream);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamCreate success, pStream ptr: %p\n", stream);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, stream);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDA_STREAM_CREATE);
            stream = (cudaStream_t)serverEp->GetHandleVirAddr(stream, CUDA_STREAM_CREATE);
            resBuf.Push64BitPointer(stream);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamCreate failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}


DEFINE_SERVER_AM_CALLBACK(cudaStreamDestroyHandle) {
    tool::Logging(myName, "CUDA_STREAM_DESTROY\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>(), true);
    cudaError_t     exit_code   = cudaStreamDestroy(stream);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamDestroy success, Stream ptr: %p\n", stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamDestroy failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}


DEFINE_SERVER_AM_CALLBACK(cudaStreamCreateWithFlagsHandle) {
    tool::Logging(myName, "CUDA_STREAM_CREATE_WITH_FLAGS\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        virAddr     = reqBuf.Pop<uint64_t>();
    cudaStream_t    stream;
    unsigned int    flags       = reqBuf.Pop<uint>();
    cudaError_t     exit_code   = cudaStreamCreateWithFlags(&stream, flags);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamCreateWithFlags success, Stream ptr: %p, flags=%u\n", stream, flags);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, stream);
        }
        else {        
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDA_STREAM_CREATE_WITH_FLAGS);
            stream = (cudaStream_t)serverEp->GetHandleVirAddr(stream, CUDA_STREAM_CREATE_WITH_FLAGS);
            resBuf.Push64BitPointer(stream);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamCreateWithFlags failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}
// end

DEFINE_SERVER_AM_CALLBACK(cudaStreamCreateWithPriorityHandle) {
    tool::Logging(myName, "CUDA_STREAM_CREATE_WITH_PRIORITY\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        virAddr     = reqBuf.Pop<uint64_t>();
    cudaStream_t    stream;
    unsigned int    flags       = reqBuf.Pop<uint>();
    int             priority    = reqBuf.Pop<int>();
    cudaError_t     exit_code   = cudaStreamCreateWithPriority(&stream, flags, priority);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamCreateWithPriority success, Stream ptr: %p, flags = %u, priority = %d\n", stream, flags, priority);
        if (virAddr != 0) {
            serverEp->SetHandleVirAddr(virAddr, stream);
        }
        else {
            RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
            resBuf.PushRequestType(CUDA_STREAM_CREATE_WITH_PRIORITY);
            stream = (cudaStream_t)serverEp->GetHandleVirAddr(stream, CUDA_STREAM_CREATE_WITH_PRIORITY);
            resBuf.Push64BitPointer(stream);
            serverEp->SendResponse(&resBuf, &param->reply_ep);
        }
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamCreateWithPriority failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaStreamWaitEventHandle) {
    tool::Logging(myName, "CUDA_STREAM_WAIT_EVENT\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    uint64_t        eventVirAddr= reqBuf.Pop<uint64_t>();
    cudaEvent_t     event       = (cudaEvent_t)serverEp->GetHandle(eventVirAddr);
    uint            flags       = reqBuf.Pop<uint>();
    if (event == NULL) {
        tool::Logging(LOG_ERROR, myName, "cudaStreamWaitEvent warning: eventPtr is NULL(virAddr=%p), streamPtr=%p\n", eventVirAddr, stream);
        return UCS_OK;
    }

    cudaError_t     exit_code   = cudaStreamWaitEvent(stream, event, flags);
    if (exit_code == cudaSuccess) {
        bool isCommEvent = false;
#ifdef GV_BACKUP
        Handle_t* handle = serverEp->GetHandleInfo(eventVirAddr);
        isCommEvent = (serverEp->commStream_) && (serverEp->commStream_ == (cudaStream_t)handle->stream);
        if (isCommEvent) {
            boost::unique_lock<boost::mutex> lock(serverEp->eventWatchedSync_.mutex);
            serverEp->watchedEventsList_.push_back(event);
        }
#endif // GV_BACKUP
        tool::Logging(myName, "cudaStreamWaitEvent success, eventPtr=%p(isCommEvent=%d), streamPtr=%p, flags = %u\n", event, isCommEvent, stream, flags);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamWaitEvent failed: %s, eventPtr=%p(virAddr=%p), streamPtr=%p\n", cudaGetErrorName(exit_code), event, eventVirAddr, stream);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaStreamSynchronizeHandle) {
    tool::Logging(myName, "CUDA_STREAM_SYNCHRONIZE\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;

#ifdef GV_BACKUP
    // debug:
    // if (serverEp->ckptCnt >= 2) {
    // // if (serverEp->curIter_ >= BACKUP_PERIOD + 1) {
    // // if (serverEp->curIter_ >= BACKUP_PERIOD + BACKUP_PERIOD - 5) {
    //     tool::Logging(LOG_INFO, myName, "Ready to close the server endpoint object.\n");
    //     // {
    //     //     boost::unique_lock<boost::mutex> lock(serverEp->backupSync_.mutex);
    //     //     serverEp->backupSync_.cv.wait(lock, [serverEp] { return !serverEp->bufferReady_; });
    //     // }
    //     serverEp->stateBackup_.Wait();

    //     for (const auto stream : serverEp->streamList_) {
    //         cudaStreamSynchronize(stream);
    //     }

    // /*
    //     size_t totalTensorsSize = 0;
    //     for (const auto& tensor : serverEp->trainTensors_) {
    //         totalTensorsSize += tensor.size;
    //     }
    //     totalTensorsSize = totalTensorsSize * BACKUP_PERIOD;
    //     std::string fileName = serverEp->backupFilePath_ + ".tmp";
    //     std::ofstream ofs(fileName.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
    //     if (!ofs) {
    //         throw std::runtime_error("Opening file failed: " + fileName);
    //     }
    //     ofs.seekp(totalTensorsSize - 1);
    //     ofs.write("", 1);
    //     ofs.close();        
    //     boost::interprocess::file_mapping fileMapping(fileName.c_str(), boost::interprocess::read_write);
    //     boost::interprocess::mapped_region mappingRegion(fileMapping, boost::interprocess::read_write, 0, totalTensorsSize);
    //     auto start = std::chrono::high_resolution_clock::now();
    //     std::memcpy(mappingRegion.get_address(), serverEp->backupMemcpyBuffer_, totalTensorsSize);
    //     auto end = std::chrono::high_resolution_clock::now();
    //     tool::Logging(LOG_INFO, myName, "training data is written to file(%s) with size %zu bytes in %f seconds\n", fileName.c_str(), totalTensorsSize, std::chrono::duration<double>(end - start).count());
    // */

    //     // exit(EXIT_FAILURE);
        
    //     serverEp->connStatus_.isClosed = true;
    //     for (int i = 0; i < 3; i++) serverEp->shmQueSizes_[i]->SaveState();
    //     return UCS_ERR_CONNECTION_RESET;
    // }
#ifdef GV_MSGHANDLER
    // if (serverEp->MSGStopFlag_) {
        if (serverEp->MSGStopFlag_ && !serverEp->msgWaitFlag_){
            tool::Logging(LOG_INFO, myName, "Ready to close the server endpoint object.\n");
            serverEp->msgResetCnt = serverEp->ckptCnt;
            serverEp->msgWaitFlag_ = true;
        } 
        else if (serverEp->MSGStopFlag_ && serverEp->msgWaitFlag_ && (serverEp->ckptCnt >= serverEp->msgResetCnt + 1)) {
        // if (serverEp->curIter_ >= BACKUP_PERIOD + 1) {
        // if (serverEp->curIter_ >= BACKUP_PERIOD + BACKUP_PERIOD - 5) {
            tool::Logging(LOG_INFO, myName, "Ready to close the server endpoint object.\n");
            // {
            //     boost::unique_lock<boost::mutex> lock(serverEp->backupSync_.mutex);
            //     serverEp->backupSync_.cv.wait(lock, [serverEp] { return !serverEp->bufferReady_; });
            // }
            serverEp->stateBackup_.Wait();
    
            for (const auto stream : serverEp->streamList_) {
                cudaStreamSynchronize(stream);
            }
    
        /*
            size_t totalTensorsSize = 0;
            for (const auto& tensor : serverEp->trainTensors_) {
                totalTensorsSize += tensor.size;
            }
            totalTensorsSize = totalTensorsSize * BACKUP_PERIOD;
            std::string fileName = serverEp->backupFilePath_ + ".tmp";
            std::ofstream ofs(fileName.c_str(), std::ios::binary | std::ios::out | std::ios::trunc);
            if (!ofs) {
                throw std::runtime_error("Opening file failed: " + fileName);
            }
            ofs.seekp(totalTensorsSize - 1);
            ofs.write("", 1);
            ofs.close();        
            boost::interprocess::file_mapping fileMapping(fileName.c_str(), boost::interprocess::read_write);
            boost::interprocess::mapped_region mappingRegion(fileMapping, boost::interprocess::read_write, 0, totalTensorsSize);
            auto start = std::chrono::high_resolution_clock::now();
            std::memcpy(mappingRegion.get_address(), serverEp->backupMemcpyBuffer_, totalTensorsSize);
            auto end = std::chrono::high_resolution_clock::now();
            tool::Logging(LOG_INFO, myName, "training data is written to file(%s) with size %zu bytes in %f seconds\n", fileName.c_str(), totalTensorsSize, std::chrono::duration<double>(end - start).count());
        */
    
            // exit(EXIT_FAILURE);
            
            serverEp->connStatus_.isClosed = true;
            for (int i = 0; i < 3; i++) serverEp->shmQueSizes_[i]->SaveState();
            return UCS_ERR_CONNECTION_RESET;
        }

    #endif // GV_MSGHANDLER
#endif // GV_BACKUP

    cudaError_t     exit_code   = cudaStreamSynchronize(stream);
    RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
    resBuf.PushRequestType(CUDA_STREAM_SYNCHRONIZE);
    resBuf.Push(exit_code);
    serverEp->SendResponse(&resBuf, &param->reply_ep);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamSynchronize success, pStream=%p\n",stream);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamSynchronize failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaStreamIsCapturingHandle) {
    tool::Logging(myName, "CUDA_STREAM_IS_CAPTURING\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    enum cudaStreamCaptureStatus pCaptureStatus;
    cudaError_t     exit_code   = cudaStreamIsCapturing(stream, &pCaptureStatus);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamIsCapturing success, pStream=%p, pCaptureStatus=%d\n",stream, pCaptureStatus);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_STREAM_IS_CAPTURING);
        resBuf.Push(pCaptureStatus);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamIsCapturing failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}
// end


DEFINE_SERVER_AM_CALLBACK(cudaStreamGetCaptureInfoHandle) {
    tool::Logging(myName, "CUDA_STREAM_GET_CAPTURE_INFO\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    // debug: 这里pop出来的是pStream, 而非stream
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    enum cudaStreamCaptureStatus pCaptureStatus;
    unsigned long long pId;
    cudaError_t     exit_code   = cudaStreamGetCaptureInfo(stream, &pCaptureStatus, &pId);
    if (exit_code == cudaSuccess) {
        tool::Logging(myName, "cudaStreamGetCaptureInfo success, streamPtr=%p, pCaptureStatus=%d, pId=%llu\n",stream, pCaptureStatus, pId);
        // send pCaptureStatus and pId of the stream back
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_STREAM_GET_CAPTURE_INFO);
        resBuf.Push(pCaptureStatus);
        resBuf.Push(pId); //todo: handle mapping
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaStreamGetCaptureInfo failed: %s\n", cudaGetErrorName(exit_code));
        return UCS_ERR_IO_ERROR;
    }
}




#include "../../include/serverEndpoint.h"

static const char* myName = "CUDARuntimeOtherHandle";

DEFINE_SERVER_AM_CALLBACK(cudaOccupancyMaxActiveBlocksPerMultiprocessorHandle) {
    tool::Logging(myName, "CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        hostFun     = reqBuf.Pop<uint64_t>();
    int             blockSize   = reqBuf.Pop<int>();
    size_t          dynSMemSize = reqBuf.Pop<size_t>();

    CUfunction      cuFunc      = NULL;
    auto            it          = serverEp->_cuInfoMap.mapHost2CuFunc->find(hostFun);
    if (it == serverEp->_cuInfoMap.mapHost2CuFunc->end()) {
        tool::Logging(LOG_ERROR, myName, "cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags failed: unknown function(%p)\n", hostFun);
        return UCS_ERR_IO_ERROR;
    }
    else {
        cuFunc = it->second;    
    }

    cudaSetDevice(serverEp->curDev_);

    int             numBlocks   = 0;
    CUresult        exit_code   = cuOccupancyMaxActiveBlocksPerMultiprocessor(&numBlocks, cuFunc, blockSize,  dynSMemSize);
    if (exit_code == CUDA_SUCCESS) {
        tool::Logging(myName, "cudaOccupancyMaxActiveBlocksPerMultiprocessor success, numBlocks = %d\n", numBlocks);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR);
        resBuf.Push(numBlocks);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        const char* errorStr;
        cuGetErrorString(exit_code, &errorStr);
        tool::Logging(LOG_ERROR, myName, "cudaOccupancyMaxActiveBlocksPerMultiprocessor failed: %s\n", errorStr);
        return UCS_ERR_IO_ERROR;
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlagsHandle) {
    tool::Logging(myName, "CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR_WITH_FLAGS\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        hostFun     = reqBuf.Pop<uint64_t>();
    int             blockSize   = reqBuf.Pop<int>();
    size_t          dynSMemSize = reqBuf.Pop<size_t>();
    unsigned int    flags       = reqBuf.Pop<unsigned int>();

    CUfunction      cuFunc      = NULL;
    auto            it          = serverEp->_cuInfoMap.mapHost2CuFunc->find(hostFun);
    if (it == serverEp->_cuInfoMap.mapHost2CuFunc->end()) {
        tool::Logging(LOG_ERROR, myName, "cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlags failed: unknown function(%p)\n", hostFun);
        return UCS_ERR_IO_ERROR;
    }
    else {
        cuFunc = it->second;    
    }

    cudaSetDevice(serverEp->curDev_);

    int             numBlocks   = 0;
    CUresult        exit_code   = cuOccupancyMaxActiveBlocksPerMultiprocessorWithFlags(&numBlocks, cuFunc, blockSize, dynSMemSize, flags);
    if (exit_code == CUDA_SUCCESS) {
        tool::Logging(myName, "cudaOccupancyMaxActiveBlocksPerMultiprocessor with Flags success, numBlocks = %d\n", numBlocks);
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_OCCUPANCY_MAX_ACTIVE_BLOCKS_PER_MULTIPROCESSOR_WITH_FLAGS);
        resBuf.Push(numBlocks);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        const char* errorStr;
        cuGetErrorString(exit_code, &errorStr);
        tool::Logging(LOG_ERROR, myName, "cudaOccupancyMaxActiveBlocksPerMultiprocessor with Flags failed: %s\n", errorStr);
        return UCS_ERR_IO_ERROR;
    }
}
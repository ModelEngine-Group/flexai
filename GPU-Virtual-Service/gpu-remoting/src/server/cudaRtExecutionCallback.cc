#include "../../include/serverEndpoint.h"

static const char* myName = "CUDARuntimeExecutionHandle";

DEFINE_SERVER_AM_CALLBACK(cudaLaunchKernelHandle) {
    tool::Logging(myName, "CUDA_LAUNCH_KERNEL\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    uint64_t        func        = reqBuf.Pop<uint64_t>();
    char*           deviceName  = reqBuf.AssignCString();
    dim3            gridDim     = reqBuf.Pop<dim3>();
    dim3            blockDim    = reqBuf.Pop<dim3>();
    uint8_t*        paraValList = reqBuf.AssignAddrForAll<uint8_t>();
    size_t          sharedMem   = reqBuf.Pop<size_t>();
    cudaStream_t    stream      = (cudaStream_t)serverEp->GetHandle(reqBuf.Pop<uint64_t>());
    stream = (stream == NULL) ? serverEp->defaultStream_ : stream;
    size_t          paramNum    = reqBuf.Pop<size_t>();
    uint16_t*       paramOffsets= reqBuf.AssignAddrForAll<uint16_t>();
    uint16_t*       paramSizes  = reqBuf.AssignAddrForAll<uint16_t>();

    CUfunction      cuFunc      = NULL;
    auto            it          = serverEp->_cuInfoMap.mapHost2CuFunc->find(func);
    if (it == serverEp->_cuInfoMap.mapHost2CuFunc->end()) {
        tool::Logging(LOG_ERROR, myName, "cudaLaunchKernel failed: unknown function(%p)\n", func);
        return UCS_ERR_IO_ERROR;
    }
    else {
        cuFunc = it->second;    
    }

#ifdef GV_MEMORY_PTX
    if (serverEp->_cuInfoMap.ptxExtractor->_finished == false) {
        serverEp->_cuInfoMap.ptxExtractor->_readyClosed = true;
        while (serverEp->_cuInfoMap.ptxExtractor->_finished == false) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    std::vector<std::pair<size_t, size_t>>* dev_ptr_list = NULL;
    auto            newIt       = serverEp->_cuInfoMap.mapDevName2DevPtr->find(deviceName);
    if (newIt == serverEp->_cuInfoMap.mapDevName2DevPtr->end()) {
        tool::Logging(LOG_ERROR, myName, "cudaLaunchKernel failed: unknown kernel name(%s)\n", deviceName);
        exit(EXIT_FAILURE);
        // return UCS_ERR_IO_ERROR;
    }
    else {
        dev_ptr_list = (std::vector<std::pair<size_t, size_t>>*)newIt->second;
        tool::Logging(myName, "cudaLaunchKernel: kernelName=%s, devPtrListSize=%zu\n", deviceName, dev_ptr_list->size());
    }
#endif // GV_MEMORY_PTX

#ifdef GV_MEMORY
    bool savedDevPtrList = true;
    // std::vector<std::pair<size_t, size_t>>* dev_ptr_list = NULL;
    std::set<std::pair<size_t, size_t>>* dev_ptr_set = NULL;
    auto            newIt       = serverEp->_cuInfoMap.mapDevName2DevPtr->find(deviceName);
    if (newIt == serverEp->_cuInfoMap.mapDevName2DevPtr->end()) {
        // dev_ptr_list = new std::vector<std::pair<size_t, size_t>>();
        dev_ptr_set = new std::set<std::pair<size_t, size_t>>();
        serverEp->_cuInfoMap.mapDevName2DevPtr->insert({deviceName, (uint64_t)dev_ptr_set});
        savedDevPtrList = false;
    }
    else {
        dev_ptr_set = (std::set<std::pair<size_t, size_t>>*)newIt->second;
        tool::Logging(myName, "cudaLaunchKernel: kernelName=%s, devPtrListSize=%zu\n", deviceName, dev_ptr_set->size());
        savedDevPtrList = !(serverEp->_cuInfoMap.isFirstIter);
    }
#endif // GV_MEMORY

    void**          paramPtrList= (void**)malloc(paramNum * sizeof(void*));
    size_t          ptrIdx  = 0;
    for (size_t i = 0; i < paramNum; i++) {
        paramPtrList[i] = paraValList + paramOffsets[i];
        tool::Logging(myName, "param[%zu]: paramPtr=%p, val=%p, paramSize=%d\n", i, (uint64_t*)paramPtrList[i], *(uint64_t*)paramPtrList[i], paramSizes[i]);

#ifdef GV_MEMORY
        if (paramSizes[i] < sizeof(uint64_t)) {
            continue;
        }
        if (!savedDevPtrList) {
            size_t paramIntraOffset = 0;
            while (paramIntraOffset <= paramSizes[i] - sizeof(uint64_t)){
                uint64_t* paramIntraPtr = (uint64_t*)((uint64_t)paramPtrList[i] + paramIntraOffset);
                uint64_t  tmpAddr       = *paramIntraPtr;
                tool::Logging(myName, "\t paramIntraOffset=%zu, tmpAddr=%p\n", paramIntraOffset, (void*)tmpAddr);
                uint64_t realPtr = 0;
                if (serverEp->_cuInfoMap.blockManager->FindByVirAddr(tmpAddr, realPtr) == -1) {
                    tool::Logging(myName, "\t warning - tmpAddr(%p) is not in blockManager\n", tmpAddr);
                    paramIntraOffset ++;
                    continue;
                }
                *paramIntraPtr = (uint64_t)realPtr;
                tool::Logging(myName, "\t paramIntraOffset=%zu, devPtr=%p\n", paramIntraOffset, *paramIntraPtr);
                dev_ptr_set->insert({i, paramIntraOffset});
                paramIntraOffset += sizeof(uint64_t);
            }
        }
        else {
            std::vector<std::pair<size_t, size_t>> dev_ptr_list(dev_ptr_set->begin(), dev_ptr_set->end());
            while (ptrIdx < dev_ptr_list.size()) {
                if ((dev_ptr_list)[ptrIdx].first != i) {
                    break;
                }
                size_t paramIntraOffset = (dev_ptr_list)[ptrIdx].second;
                uint64_t* paramIntraPtr = (uint64_t*)((uint64_t)paramPtrList[i] + paramIntraOffset);
                uint64_t  usrVirtAddr   = *paramIntraPtr;
                tool::Logging(LOG_REGS, myName, "\t paramIntraOffset=%zu, usrVirtAddr=%p\n", paramIntraOffset, usrVirtAddr);
                uint64_t realPtr = 0;
                ptrIdx++;
                if (serverEp->_cuInfoMap.blockManager->FindByVirAddr(usrVirtAddr, realPtr) == -1) {
                    tool::Logging(LOG_REGS, myName, "\t warning - usrVirtAddr(%p) is not in blockManager\n", usrVirtAddr);
                    continue;
                }
                *paramIntraPtr = (uint64_t)realPtr;
                tool::Logging(myName, "\t paramIntraOffset=%zu, devPtr=%p\n", paramIntraOffset, *paramIntraPtr);
            }
        }
#endif // GV_MEMORY

#ifdef GV_MEMORY_PTX 
        if (strcmp(deviceName, "_ZN2at6native6sbtopk10gatherTopKIfjLi2ELb0EEEvNS_4cuda6detail10TensorInfoIT_T0_EES7_S7_bS7_S7_S8_S7_NS5_IlS7_EES7_PS6_" ) == 0 && i == 0) {
            tool::Logging(myName, "param[%zu]: paramPtr=%p, val=%p, paramSize=%d\n", i, (uint64_t*)paramPtrList[i], *(uint64_t*)paramPtrList[i], paramSizes[i]);
            size_t paramIntraOffset = 0;
            uint64_t* paramIntraPtr = (uint64_t*)((uint64_t)paramPtrList[i] + paramIntraOffset);
            uint64_t  usrVirtAddr   = *paramIntraPtr; // get the devPtr from current param #i
            tool::Logging(myName, "\t paramIntraOffset=%zu, usrVirtAddr=%p\n", paramIntraOffset, usrVirtAddr);
            if (usrVirtAddr == 0) { // the devPtr can be NULL
                continue;
            }
            uint64_t blockID        = GET_BLOCK_ID(usrVirtAddr);
            uint64_t blockIntraOffset = GET_BLOCK_INTER_OFFSET(usrVirtAddr);
            *paramIntraPtr = (uint64_t)serverEp->GetDevPtr(usrVirtAddr);
            tool::Logging(myName, "\t blockID=%p, blockIntraOffset=%zu, devPtr=%p\n", blockID, blockIntraOffset, *paramIntraPtr);
        }

        while (ptrIdx < dev_ptr_list->size()) {
            if ((*dev_ptr_list)[ptrIdx].first != i) { // next devPtr is not in current param #i
                break;
            }
            size_t paramIntraOffset = (*dev_ptr_list)[ptrIdx].second;
            uint64_t* paramIntraPtr = (uint64_t*)((uint64_t)paramPtrList[i] + paramIntraOffset);
            uint64_t  usrVirtAddr   = *paramIntraPtr; // get the devPtr from current param #i
            tool::Logging(myName, "\t paramIntraOffset=%zu, usrVirtAddr=%p\n", paramIntraOffset, usrVirtAddr);
            ptrIdx++; // ready move to next devPtr, which may be in the same param #i
            if (usrVirtAddr == 0) { // the devPtr can be NULL
                continue;
            }

            uint64_t blockID        = GET_BLOCK_ID(usrVirtAddr);
            uint64_t blockIntraOffset = GET_BLOCK_INTER_OFFSET(usrVirtAddr);
            if (blockID <= 0 || blockID >= serverEp->_cuInfoMap.blockInfoList.size()) {
                tool::Logging(myName, "\t warning - blockID(%p), current blockNum=%zu\n", blockID, serverEp->_cuInfoMap.blockInfoList.size());
                // exit(EXIT_FAILURE);
                continue;
            }
            if (blockIntraOffset >= serverEp->_cuInfoMap.blockInfoList[blockID].size) {
                tool::Logging(myName, "\t cudaLaunchKernel: warning - blockIntraOffset(%p), current block size=%zu\n", blockIntraOffset, serverEp->_cuInfoMap.blockInfoList[blockID].size);
                // exit(EXIT_FAILURE);
                continue;
            }
            *paramIntraPtr = (uint64_t)serverEp->GetDevPtr(usrVirtAddr);
            tool::Logging(myName, "\t blockID=%p, blockIntraOffset=%zu, devPtr=%p\n", blockID, blockIntraOffset, *paramIntraPtr);
        }
#endif // GV_MEMORY_PTX

    }

    cudaSetDevice(serverEp->curDev_); // explicitly set the device and make sure the context is valid

    CUresult        exit_code   = cuLaunchKernel(cuFunc, gridDim.x, gridDim.y, gridDim.z, 
                                                blockDim.x, blockDim.y, blockDim.z, sharedMem,
                                                (CUstream)stream, paramPtrList, NULL);
    if (exit_code == CUDA_SUCCESS) {
        tool::Logging(myName, "cudaLaunchKernel success: %s\n", deviceName);
#ifdef GV_MEMORY_PTX
        cudaError_t sync_code = cudaDeviceSynchronize();
        if (sync_code != cudaSuccess) {
            tool::Logging(LOG_ERROR, myName, "cudaDeviceSynchronize (after launchKernel: %s) failed: %s\n", deviceName, cudaGetErrorName(sync_code));
            exit(EXIT_FAILURE);
        }
#endif // GV_MEMORY_PTX
        if (serverEp->curIter_ <= 1 // necessary to synchronize the non-blocking stream for the first iterations
         || serverEp->recoveryFlag_ // synchronize the replaying kernel (e.g., indexSelectLargeIndex)
        ) { 
            cudaError_t sync_code = cudaStreamSynchronize(stream);
            if (sync_code != cudaSuccess) {
                tool::Logging(LOG_ERROR, myName, "cudaStreamSynchronize (after launchKernel: %s) failed: %s\n", deviceName, cudaGetErrorName(sync_code));
                exit(EXIT_FAILURE);
            }
        }
        return UCS_OK;
    }
    else {
        const char* errorStr;
        cuGetErrorString(exit_code, &errorStr);
        tool::Logging(LOG_ERROR, myName, "cudaLaunchKernel failed: %s\n", errorStr);
        // return UCS_ERR_IO_ERROR;
        exit(EXIT_FAILURE);
    }
}

DEFINE_SERVER_AM_CALLBACK(cudaFuncGetAttributesHandle) {
    tool::Logging(myName, "CUDA_FUNC_GET_ATTRIBUTES\n");
    ServerEndpoint* serverEp    = (ServerEndpoint*) arg;
    RequestIOV      reqBuf      = RequestIOV(header, header_length, data);
    struct cudaFuncAttributes attr;
    uint64_t        func        = reqBuf.Pop<uint64_t>();

    CUfunction      cuFunc      = NULL;
    auto            it          = serverEp->_cuInfoMap.mapHost2CuFunc->find(func);
    if (it == serverEp->_cuInfoMap.mapHost2CuFunc->end()) {
        tool::Logging(LOG_ERROR, myName, "cudaLaunchKernel failed: unknown function(%p)\n", func);
        return UCS_ERR_IO_ERROR;
    }
    else {
        cuFunc = it->second;    
    }

    int             exit_code   = CUDA_SUCCESS;
    int             shrSzBytes  = 0;
    int             conSzBytes  = 0;
    int             locSzBytes  = 0;
    exit_code |= cuFuncGetAttribute(&shrSzBytes, CU_FUNC_ATTRIBUTE_SHARED_SIZE_BYTES, cuFunc);
    exit_code |= cuFuncGetAttribute(&conSzBytes, CU_FUNC_ATTRIBUTE_CONST_SIZE_BYTES, cuFunc);
    exit_code |= cuFuncGetAttribute(&locSzBytes, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, cuFunc);
    attr.sharedSizeBytes        = shrSzBytes;
    attr.constSizeBytes         = conSzBytes;
    attr.localSizeBytes         = locSzBytes;
    exit_code |= cuFuncGetAttribute((int*)&attr.maxThreadsPerBlock, CU_FUNC_ATTRIBUTE_MAX_THREADS_PER_BLOCK, cuFunc);
    exit_code |= cuFuncGetAttribute((int*)&attr.numRegs, CU_FUNC_ATTRIBUTE_NUM_REGS, cuFunc);
    exit_code |= cuFuncGetAttribute((int*)&attr.ptxVersion, CU_FUNC_ATTRIBUTE_PTX_VERSION, cuFunc);
    exit_code |= cuFuncGetAttribute((int*)&attr.binaryVersion, CU_FUNC_ATTRIBUTE_BINARY_VERSION, cuFunc);
    exit_code |= cuFuncGetAttribute((int*)&attr.cacheModeCA, CU_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT, cuFunc);
    exit_code |= cuFuncGetAttribute((int*)&attr.maxDynamicSharedSizeBytes, CU_FUNC_ATTRIBUTE_MAX_DYNAMIC_SHARED_SIZE_BYTES, cuFunc);
    exit_code |= cuFuncGetAttribute((int*)&attr.preferredShmemCarveout, CU_FUNC_ATTRIBUTE_PREFERRED_SHARED_MEMORY_CARVEOUT, cuFunc);


    if (exit_code == CUDA_SUCCESS) {
        tool::Logging(myName, "cudaFuncGetAttributes success\n");
        RequestIOV resBuf = RequestIOV(reqBuf.GetThreadID());
        resBuf.PushRequestType(CUDA_FUNC_GET_ATTRIBUTES);
        resBuf.Push(attr);
        serverEp->SendResponse(&resBuf, &param->reply_ep);
        return UCS_OK;
    }
    else {
        tool::Logging(LOG_ERROR, myName, "cudaFuncGetAttributes failed\n");
        return UCS_ERR_IO_ERROR;
    }
}
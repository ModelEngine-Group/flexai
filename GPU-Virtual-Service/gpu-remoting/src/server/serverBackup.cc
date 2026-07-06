#include "../../include/serverEndpoint.h"

void ServerEndpoint::Backup2Memory() {
    const char* myName = "Backup";
    tool::Logging(LOG_DEBUG, myName, "ready to backup the memory blocks and the handle mappings.\n");

    auto start_total = std::chrono::high_resolution_clock::now();

    // /* Wait for the sub thread to signal that the buffer is ready to be overwritten */
    // {
    //     boost::unique_lock<boost::mutex> lock(backupSync_.mutex);
    //     backupSync_.cv.wait(lock, [this] { return !bufferReady_; });
    // }

    stateBackup_.Wait();

    /* prepare the backup buffer and prepare */
    auto start_prepare = std::chrono::high_resolution_clock::now();
    _cuInfoMap.handleManager->Shrink(); // remove the invalid handles
    size_t totalDataBytes = 0, memcpyDataBytes = 0;
    // const std::vector<Handle_t>& handles = _cuInfoMap.handleManager->GetHandleInfoList();
    const std::vector<std::pair<size_t, Handle_t>>& handles = _cuInfoMap.handleManager->GetValidHandles();
    const std::vector<Block_t>& blocks = _cuInfoMap.blockManager->GetBlocks();

    std::vector<TensorInfo_t> sortedTensors = trainTensors_;
    std::sort(sortedTensors.begin(), sortedTensors.end(),
              [](const TensorInfo_t& a, const TensorInfo_t& b) {
                  return a.devPtr < b.devPtr;
              });
    std::vector<std::vector<const TensorInfo_t*>> block2tensors(blocks.size());
    for (size_t i = 0; i < sortedTensors.size(); i++) {
        uint64_t realAddr = 0;
        int blockIdx = _cuInfoMap.blockManager->FindByVirAddr((uint64_t)sortedTensors[i].devPtr, realAddr);
        if (blockIdx != -1) {
            const Block_t& tmpBlock = blocks[blockIdx];
            sortedTensors[i].devPtr = (void*)realAddr;
            if (tmpBlock.essential && realAddr + sortedTensors[i].size <= tmpBlock.devPtr + tmpBlock.size) {
                block2tensors[blockIdx].push_back(&sortedTensors[i]);
                tool::Logging(LOG_DEBUG, myName, "block[%d](addr=%p, size=%zu) has tensor[%zu](%p) with size %zu\n", blockIdx, (void*)tmpBlock.devPtr, tmpBlock.size, i, sortedTensors[i].devPtr, sortedTensors[i].size);
                memcpyDataBytes += sortedTensors[i].size;
            }
            else if (tmpBlock.essential == false) {
                tool::Logging(LOG_DEBUG, myName, "block[%d](%p) is not essential\n", blockIdx, (void*)tmpBlock.devPtr);
            }
            else {
                tool::Logging(LOG_DEBUG, myName, "tensor[%zu](%p) is not in block[%d](%p)\n", i, sortedTensors[i].devPtr, blockIdx, (void*)tmpBlock.devPtr);
            }     
        }
    }
    // size_t tmpOffset = 0;
    for (size_t blockIdx = 0; blockIdx < blocks.size(); blockIdx++) {
        if (blocks[blockIdx].valid == false || blocks[blockIdx].essential == false) {
            continue;
        }
        // tool::Logging(LOG_INFO, myName, "offset=%zu, prepare to backup block[%zu]: devPtr=%p, size=%zu\n", tmpOffset, blockIdx, (void*)blocks[blockIdx].devPtr, blocks[blockIdx].size);
        totalDataBytes += blocks[blockIdx].size;
        // tmpOffset += blocks[blockIdx].size;
    }

    size_t curBackupSize = sizeof(lastCopyLen_) + sizeof(lastCopyType_) + sizeof(size_t) * 3 + sizeof(handles[0]) * handles.size()
                         + sizeof(Block_t) * blocks.size() + sizeof(TensorInfo_t) * trainTensors_.size()
                         + (totalDataBytes - memcpyDataBytes);
                        //  + totalDataBytes;
    tool::Logging(LOG_DEBUG, myName, "prepare the backup buffer with %zu bytes (dataBytes: %zu, memcpyBytes: %zu)\n", curBackupSize, totalDataBytes, memcpyDataBytes);
    if (serverState_.hostPtr == NULL || serverState_.size < curBackupSize) {
        bufferResized_ = true;
        serverState_.size = ALIGN_UP(curBackupSize); // align to 4KB
        if (serverState_.hostPtr != NULL) {
            // free(serverState_.hostPtr);
            cudaFreeHost(serverState_.hostPtr);
        }
        cudaError_t exit_code = cudaMallocHost(&serverState_.hostPtr, serverState_.size);
        if (exit_code != cudaSuccess) {
            tool::Logging(LOG_ERROR, myName, "cudaMallocHost failed: %s\n", cudaGetErrorString(exit_code));
            exit(EXIT_FAILURE);
        }
    }
    if (streamList_.size() == 0) {
        streamList_.resize(BACKUP_STREAM_NUM, NULL);
        for (int i = 0; i < streamList_.size(); i++) {
            cudaStreamCreateWithFlags(&streamList_[i], cudaStreamNonBlocking);
        }
    }
    auto end_prepare = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_prepare = end_prepare - start_prepare;
    tool::Logging(LOG_INFO, myName, "backup buffer is prepared with %zu bytes (isResided: %d) in %f seconds\n", serverState_.size, bufferResized_, duration_prepare.count());


    /* backup the mappings for the memory blocks and the handles */
    auto start_backup = std::chrono::high_resolution_clock::now();
    size_t bufferOffset = 0;
    size_t handleNum = handles.size();
    size_t blockNum = blocks.size();
    size_t memcpyCnt = trainTensors_.size();
    memcpy(serverState_.hostPtr + bufferOffset, &lastCopyLen_, sizeof(lastCopyLen_));
    bufferOffset += sizeof(lastCopyLen_);
    memcpy(serverState_.hostPtr + bufferOffset, &lastCopyType_, sizeof(lastCopyType_));
    bufferOffset += sizeof(lastCopyType_);
    memcpy(serverState_.hostPtr + bufferOffset, &handleNum, sizeof(size_t));
    bufferOffset += sizeof(size_t);
    memcpy(serverState_.hostPtr + bufferOffset, &blockNum, sizeof(size_t));
    bufferOffset += sizeof(size_t);
    memcpy(serverState_.hostPtr + bufferOffset, &memcpyCnt, sizeof(size_t));
    bufferOffset += sizeof(size_t);
    // memcpy(serverState_.hostPtr + bufferOffset, handles.data(), sizeof(Handle_t) * handleNum);
    // bufferOffset += sizeof(Handle_t) * handleNum;
    memcpy(serverState_.hostPtr + bufferOffset, handles.data(), sizeof(handles[0]) * handleNum);
    bufferOffset += sizeof(handles[0]) * handleNum;
    memcpy(serverState_.hostPtr + bufferOffset, blocks.data(), sizeof(Block_t) * blockNum);
    bufferOffset += sizeof(Block_t) * blockNum;
    memcpy(serverState_.hostPtr + bufferOffset, trainTensors_.data(), sizeof(TensorInfo_t) * trainTensors_.size());
    bufferOffset += sizeof(TensorInfo_t) * trainTensors_.size();
    auto end_backup = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration_backup = end_backup - start_backup;
    tool::Logging(LOG_INFO, myName, "backup %zu (valid) handles and %zu blockInfos in %f seconds\n", handleNum, blockNum, duration_backup.count());

    /* backup the memory blocks */
    auto start = std::chrono::high_resolution_clock::now();
    size_t essentialBlockNum = 0;
    for (size_t blockIdx = 0; blockIdx < blocks.size(); blockIdx++) {
        if (blocks[blockIdx].valid == false || blocks[blockIdx].essential == false) {
            continue;
        }
        essentialBlockNum++;

        // printf("\n");
        tool::Logging(LOG_DEBUG, myName, "bufferOffset=%zu, prepare to backup block[%zu]: devPtr=%p, size=%zu\n", bufferOffset, blockIdx, (void*)blocks[blockIdx].devPtr, blocks[blockIdx].size);

        const Block_t& block = blocks[blockIdx];
        const auto& tensorsInBlock = block2tensors[blockIdx];
        int streamIdx = blockIdx % streamList_.size();
        uint8_t* blockAddr = (uint8_t*)block.devPtr;

        for (const auto& tensor : tensorsInBlock) {
            uint8_t* tensorStart = (uint8_t*)tensor->devPtr;
            uint8_t* tensorEnd = tensorStart + tensor->size;

            // copy the gap between the last tensor(or the start of the block) and the current tensor 
            size_t lastSegmentSize = tensorStart - blockAddr; 
            if (lastSegmentSize > 0) {
                cudaError_t memcpy_exit_code = cudaMemcpyAsync(serverState_.hostPtr + bufferOffset, (void*)blockAddr, lastSegmentSize, cudaMemcpyDeviceToHost, streamList_[streamIdx]);
                if (memcpy_exit_code != cudaSuccess) {
                    tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync failed for the memory segment in block[%zu] (%p : %p) with %zu bytes: %s\n", blockIdx, blockAddr, tensorStart, lastSegmentSize, cudaGetErrorString(memcpy_exit_code));
                    exit(EXIT_FAILURE);
                }
                else {
                    bufferOffset += lastSegmentSize;
                    tool::Logging(LOG_DEBUG, myName, "backup the memory segment in block[%zu] (%p : %p) with %zu bytes\n", blockIdx, blockAddr, tensorStart, lastSegmentSize);
                }
            }
            // move the blockAddr to the end of the current tensor
            blockAddr = tensorEnd;
        }

        // copy the remaining data in the block
        size_t remainingSize = (uint8_t*)block.devPtr+block.size - blockAddr;
        if (remainingSize > 0) {
            cudaError_t memcpy_exit_code = cudaMemcpyAsync(serverState_.hostPtr + bufferOffset, (void*)blockAddr, remainingSize, cudaMemcpyDeviceToHost, streamList_[streamIdx]);
            if (memcpy_exit_code != cudaSuccess) {
                tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync failed for the remaining memory segment in block[%zu] (%p) with %zu bytes: %s\n", blockIdx, blockAddr, remainingSize, cudaGetErrorString(memcpy_exit_code));
                tool::Logging(LOG_ERROR, myName, "bufferOffset=%zu, essentialBlockNum=%zu\n", bufferOffset, essentialBlockNum);
                exit(EXIT_FAILURE);
            }
            else {
                tool::Logging(LOG_DEBUG, myName, "backup the remaining memory segment in block[%zu] (%p) with %zu bytes\n", blockIdx, blockAddr, remainingSize);
                bufferOffset += remainingSize;
                essentialBlockNum++;
            }
        }
    }
    for (int i = 0; i < streamList_.size(); i++) {
        cudaError_t sync_code = cudaStreamSynchronize(streamList_[i]);
        if (sync_code != cudaSuccess) {
            tool::Logging(LOG_ERROR, myName, "cudaStreamSynchronize failed: %s\n", cudaGetErrorString(sync_code));
            exit(EXIT_FAILURE);
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> duration = end - start;
    tool::Logging(LOG_INFO, myName, "backup device memory success with %zu bytes (%zu essential blocks) in %f seconds\n", bufferOffset, essentialBlockNum, duration.count());

    const char* backupcsv_path = std::getenv("FLEXGV_BACKUP_CSV_PATH");
    if (backupcsv_path != NULL) {
        std::ofstream outFile;
        outFile.open(backupcsv_path, std::ios::app);
        if (outFile.tellp() == 0) {
            outFile << "Bytes,Time" << std::endl;
        }
        outFile << bufferOffset << "," << duration.count() << std::endl;
        outFile.close();
    }

    // /* Signal the sub thread that the buffer is ready to be written */
    // {
    //     boost::unique_lock<boost::mutex> lock(backupSync_.mutex);
    //     bufferReady_ = true;
    //     backupSync_.cv.notify_one();
    // }

    stateBackup_.Notify();

    // debug:
    // std::vector<Block_t>& tmpBlocks = _cuInfoMap.blockManager->GetBlocks();
    // for (size_t blockIdx = 0; blockIdx < tmpBlocks.size(); blockIdx++) {
    //     if (tmpBlocks[blockIdx].valid == false) {
    //         continue;
    //     }
    //     tool::Logging(LOG_DEBUG, myName, "blockInfoList[%zu]: devPtr=%p, size=%zu, valid=%d\n", blockIdx, (void*)tmpBlocks[blockIdx].devPtr, tmpBlocks[blockIdx].size, tmpBlocks[blockIdx].valid);
    //     cudaFree((void*)tmpBlocks[blockIdx].devPtr);
    //     tmpBlocks[blockIdx].valid = false;
    //     tmpBlocks[blockIdx].devPtr = (uint64_t)NULL;
    //     tmpBlocks[blockIdx].size = 0;
    // }
    // std::vector<Handle_t>& tmpHandles = _cuInfoMap.handleManager->GetHandleInfoList();
    // for (size_t i = 1; i < tmpHandles.size(); i++) {
    //     if (tmpHandles[i].valid == false) {
    //         continue;
    //     }
    //     tool::Logging(LOG_INFO, myName, "handleInfoList[%zu]: handlePtr=%p, type=%d, valid=%d\n", i, (void*)tmpHandles[i].handlePtr, tmpHandles[i].type, tmpHandles[i].valid);
    // }
}

void ServerEndpoint::Persist2File(const char* fileName, const uint8_t* data, size_t size) {
    try {
        if (bufferResized_) {
            bufferResized_ = false;
            /* resize or create the file */
            std::ofstream ofs(fileName, std::ios::binary | std::ios::out | std::ios::trunc);
            if (!ofs) {
                throw std::runtime_error("Opening file failed: " + std::string(fileName));
            }
            ofs.seekp(size - 1);
            ofs.write("", 1);
            ofs.close();
            tool::Logging(LOG_DEBUG, "Persist2File", "File(%s) is resized to %zu bytes\n", fileName, size);
            
            /* open the file and map to memory */
            boost::interprocess::file_mapping fileMapping(fileName, boost::interprocess::read_write);
            fileMappingRegion_ = std::make_unique<boost::interprocess::mapped_region>(fileMapping, boost::interprocess::read_write, 0, size);
        }
        
        // copy the data to the memory-mapped file
        auto start = std::chrono::high_resolution_clock::now();
        std::memcpy(fileMappingRegion_->get_address(), data, size);
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        tool::Logging(LOG_INFO, "Persist2File", "Time taken to copy data to file(%s): %f seconds\n", fileName, duration.count());
    }
    catch (const boost::interprocess::interprocess_exception& ex) {
        tool::Logging(LOG_ERROR, "Persist2File", "Writing to file failed: %s\n", ex.what());
    }
    catch (const std::exception& ex) {
        tool::Logging(LOG_ERROR, "Persist2File", "Writing to file failed: general error(%s)\n", ex.what());
    }
}

void ServerEndpoint::Backup2Storage() {
    while (true) {
        // /* Wait for main thread to signal that the buffer is ready or the thread should exit */
        // {
        //     boost::unique_lock<boost::mutex> lock(backupSync_.mutex);
        //     backupSync_.cv.wait(lock, [this] { return bufferReady_ || bufferFinished_; });
        //     if (bufferFinished_ && !bufferReady_) {
        //         break;
        //     }
        // }
        if (!stateBackup_.Check()) {
            break;
        }

        /* Write the buffer to storage */
        Persist2File(backupFilePath_.c_str(), serverState_.hostPtr, serverState_.size);

        // /* Signal the main thread that the buffer has been written */
        // {
        //     boost::unique_lock<boost::mutex> lock(backupSync_.mutex);
        //     bufferReady_ = false;
        //     backupSync_.cv.notify_one();
        // }

        stateBackup_.Notify();
    }
}

void ServerEndpoint::LoadFromStorage() {
    const char* myName = "Load";
    const char* fileName = backupFilePath_.c_str();
    tool::Logging(LOG_DEBUG, myName, "ready to load the memory blocks and the handle mappings.\n");
    if (streamList_.size() == 0) {
        streamList_.resize(BACKUP_STREAM_NUM, NULL);
        for (int i = 0; i < streamList_.size(); i++) {
            cudaStreamCreateWithFlags(&streamList_[i], cudaStreamNonBlocking);
        }
    }
    try {
/*
        std::string tmpFileName = backupFilePath_ + ".tmp";
        std::ifstream tmpofs(tmpFileName, std::ios::binary | std::ios::ate);
        if (!tmpofs.is_open()) {
            throw std::runtime_error(std::string("Unable to open file: ") + tmpFileName);
        }
        std::streamsize tmpFileSize = tmpofs.tellg();
        if (tmpFileSize < 0) {
            throw std::runtime_error("Unable to get file size");
        }
        tmpofs.close();
        size_t trainDataSize = static_cast<size_t>(tmpFileSize); 
        backupMemcpyBuffer_ = malloc(trainDataSize);
        tool::Logging(LOG_DEBUG, myName, "backupMemcpyBuffer_ is allocated with %zu bytes\n", trainDataSize);
        boost::interprocess::file_mapping tmpFile(tmpFileName.c_str(), boost::interprocess::read_only);
        boost::interprocess::mapped_region tmpRegion(tmpFile, boost::interprocess::read_only, 0, trainDataSize);
        std::memcpy(backupMemcpyBuffer_, tmpRegion.get_address(), trainDataSize);
        tool::Logging(LOG_INFO, myName, "backupMemcpyBuffer_ is loaded with %zu bytes\n", trainDataSize);
*/

        /* Get the file size */
        std::ifstream ofs(fileName, std::ios::binary | std::ios::ate);
        if (!ofs.is_open()) {
            throw std::runtime_error(std::string("Unable to open file: ") + fileName);
        }
        std::streamsize fileSize = ofs.tellg();
        if (fileSize < 0) {
            throw std::runtime_error("Unable to get file size");
        }
        ofs.close();
        size_t dataSize = static_cast<size_t>(fileSize);

        boost::interprocess::file_mapping file(fileName, boost::interprocess::read_only);
        boost::interprocess::mapped_region region(file, boost::interprocess::read_only, 0, dataSize);

        size_t bufferOffset = 0;
        size_t handleNum = 0, blockNum = 0, memcpyCnt = 0;
        memcpy(&lastCopyLen_, (uint8_t*)region.get_address() + bufferOffset, sizeof(lastCopyLen_));
        bufferOffset += sizeof(lastCopyLen_);
        memcpy(&lastCopyType_, (uint8_t*)region.get_address() + bufferOffset, sizeof(lastCopyType_));
        bufferOffset += sizeof(lastCopyType_);
        memcpy(&handleNum, (uint8_t*)region.get_address() + bufferOffset, sizeof(size_t));
        bufferOffset += sizeof(size_t);
        memcpy(&blockNum, (uint8_t*)region.get_address() + bufferOffset, sizeof(size_t));
        bufferOffset += sizeof(size_t);
        memcpy(&memcpyCnt, (uint8_t*)region.get_address() + bufferOffset, sizeof(size_t));
        bufferOffset += sizeof(size_t);
        tool::Logging(LOG_INFO, myName, "ready to load %zu (valid) handles and %zu blocks\n", handleNum, blockNum);

        // std::vector<Handle_t>& tmpHandleList = _cuInfoMap.handleManager->GetHandleInfoList();
        std::vector<std::pair<size_t, Handle_t>> tmpHandleList;
        tmpHandleList.resize(handleNum);
        // std::memcpy(tmpHandleList.data(), (uint8_t*)region.get_address() + bufferOffset, sizeof(Handle_t) * handleNum);
        std::memcpy(tmpHandleList.data(), (uint8_t*)region.get_address() + bufferOffset, sizeof(tmpHandleList[0]) * handleNum);
        // bufferOffset += sizeof(Handle_t) * handleNum;
        bufferOffset += sizeof(tmpHandleList[0]) * handleNum;
        _cuInfoMap.handleManager->Reset(tmpHandleList);
        tool::Logging(LOG_INFO, myName, "handleInfoList is successfully loaded with %zu handles\n", handleNum);
        // _cuInfoMap.handleManager->Indexing();
        // _cuInfoMap.handleManager->Shrink();

        std::vector<Block_t>& tmpBlockList = _cuInfoMap.blockManager->GetBlocks();
        tmpBlockList.resize(blockNum);
        std::memcpy(tmpBlockList.data(), (uint8_t*)region.get_address() + bufferOffset, sizeof(Block_t) * blockNum);
        bufferOffset += sizeof(Block_t) * blockNum;
        tool::Logging(LOG_INFO, myName, "blockInfoList is successfully loaded with %zu blocks\n", blockNum);

        trainTensors_.resize(memcpyCnt);
        std::memcpy(trainTensors_.data(), (uint8_t*)region.get_address() + bufferOffset, sizeof(TensorInfo_t) * memcpyCnt);
        bufferOffset += sizeof(TensorInfo_t) * memcpyCnt;

        std::vector<TensorInfo_t> sortedTensors = trainTensors_;
        std::sort(sortedTensors.begin(), sortedTensors.end(),
                  [](const TensorInfo_t& a, const TensorInfo_t& b) {
                      return a.devPtr < b.devPtr;
                  });
        std::vector<std::vector<const TensorInfo_t*>> block2tensors(tmpBlockList.size());
        for (size_t i = 0; i < sortedTensors.size(); i++) {
            uint64_t tmpAddr = 0;
            int blockIdx = _cuInfoMap.blockManager->FindByVirAddr((uint64_t)sortedTensors[i].devPtr, tmpAddr);
            if (blockIdx != -1) {
                const Block_t& tmpBlock = tmpBlockList[blockIdx];
                if (tmpBlock.essential && (uint8_t*)sortedTensors[i].devPtr + sortedTensors[i].size <= (uint8_t*)tmpBlock.start + tmpBlock.size) {
                    block2tensors[blockIdx].push_back(&sortedTensors[i]);
                } //!: the devPtr of the sortedTensors is virtual address because the real address is not available
            }
        }
        
        auto start = std::chrono::high_resolution_clock::now();
        size_t essentialBlockNum = 0;
        for (size_t blockIdx = 0; blockIdx < tmpBlockList.size(); blockIdx++) {
            if (tmpBlockList[blockIdx].valid == false) {
                continue;
            }
            Block_t* block = &tmpBlockList[blockIdx];
            void* devPtr = NULL;
            cudaError_t exit_code = cudaMalloc(&devPtr, block->size);
            if (exit_code != cudaSuccess) {
                tool::Logging(LOG_ERROR, myName, "cudaMalloc failed: %s\n", cudaGetErrorString(exit_code));
                exit(EXIT_FAILURE);
            }
            else {
                block->devPtr = (uint64_t)devPtr;
                tool::Logging(LOG_DEBUG, myName, "blockInfoList[%zu]: devPtr=%p (virAddr=%p:%p), size=%zu, valid=%d, essential=%d\n", blockIdx, (void*)block->devPtr, (void*)block->start, (uint8_t*)block->start+block->size, block->size, block->valid, block->essential);
            }

            if (tmpBlockList[blockIdx].essential == false) {
                continue;
            }
            essentialBlockNum++;
            
            const auto& tensorsInBlock = block2tensors[blockIdx];
            int streamIdx = blockIdx % streamList_.size();
            uint8_t* blockAddr = (uint8_t*)block->devPtr;

            for (const auto& tensor: tensorsInBlock) {
                uint8_t* tensorStart = (uint8_t*)devPtr + ((uint64_t)tensor->devPtr - block->start); // the real address of the tensor
                uint8_t* tensorEnd = tensorStart + tensor->size;

                // copy the gap between the last tensor(or the start of the block) and the current tensor
                size_t lastSegmentSize = tensorStart - blockAddr;
                if (lastSegmentSize > 0) {
                    cudaError_t memcpy_exit_code = cudaMemcpyAsync((void*)blockAddr, (uint8_t*)region.get_address() + bufferOffset, lastSegmentSize, cudaMemcpyHostToDevice, streamList_[streamIdx]);
                    if (memcpy_exit_code != cudaSuccess) {
                        tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync failed for the memory segment in block[%zu] (%p : %p) with %zu bytes: %s\n", blockIdx, blockAddr, tensorStart, lastSegmentSize, cudaGetErrorString(memcpy_exit_code));
                        exit(EXIT_FAILURE);
                    }
                    else {
                        bufferOffset += lastSegmentSize;
                        tool::Logging(LOG_DEBUG, myName, "load the memory segment in block[%zu] (%p : %p) with %zu bytes\n", blockIdx, blockAddr, tensorStart, lastSegmentSize);
                    }
                }
                // move the blockAddr to the end of the current tensor
                blockAddr = tensorEnd;
            }
            // copy the remaining data in the block
            size_t remainingSize = (uint8_t*)block->devPtr + block->size - blockAddr;
            if (remainingSize > 0) {
                cudaError_t memcpy_exit_code = cudaMemcpyAsync((void*)blockAddr, (uint8_t*)region.get_address() + bufferOffset, remainingSize, cudaMemcpyHostToDevice, streamList_[streamIdx]);
                if (memcpy_exit_code != cudaSuccess) {
                    tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync failed for the remaining memory segment in block[%zu] (%p) with %zu bytes: %s\n", blockIdx, blockAddr, remainingSize, cudaGetErrorString(memcpy_exit_code));
                    exit(EXIT_FAILURE);
                }
                else {
                    tool::Logging(LOG_DEBUG, myName, "load the remaining memory segment in block[%zu] (%p) with %zu bytes\n", blockIdx, blockAddr, remainingSize);
                    bufferOffset += remainingSize;
                }
            }
        }

        for (int i = 0; i < streamList_.size(); i++) {
            cudaError_t sync_code = cudaStreamSynchronize(streamList_[i]);
            if (sync_code != cudaSuccess) {
                tool::Logging(LOG_ERROR, myName, "cudaStreamSynchronize failed: %s\n", cudaGetErrorString(sync_code));
                exit(EXIT_FAILURE);
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end - start;
        tool::Logging(LOG_INFO, myName, "load device memory success with %zu bytes (%zu essential blocks) in %f seconds\n", bufferOffset, essentialBlockNum, duration.count());

        // debug:
        std::vector<Block_t>& tmpBlocks = _cuInfoMap.blockManager->GetBlocks();
        // for (size_t blockIdx = 0; blockIdx < tmpBlocks.size(); blockIdx++) {
        //     if (tmpBlocks[blockIdx].valid == false) {
        //         continue;
        //     }
        //     tool::Logging(LOG_INFO, myName, "blockInfoList[%zu]: devPtr=%p, size=%zu, valid=%d\n", blockIdx, (void*)tmpBlocks[blockIdx].devPtr, tmpBlocks[blockIdx].size, tmpBlocks[blockIdx].valid);
        // }
        // std::vector<Handle_t>& tmpHandles = _cuInfoMap.handleManager->GetHandleInfoList();
        // for (size_t i = 1; i < tmpHandles.size(); i++) {
        //     if (tmpHandles[i].valid == false) {
        //         continue;
        //     }
        //     tool::Logging(LOG_INFO, myName, "handleInfoList[%zu]: handlePtr=%p, type=%d, valid=%d\n", i, (void*)tmpHandles[i].handlePtr, tmpHandles[i].type, tmpHandles[i].valid);
        // }
    }
    catch (const boost::interprocess::interprocess_exception& ex) {
        tool::Logging(LOG_ERROR, myName, "Reading from file failed: %s\n", ex.what());
    }
    catch (const std::exception& ex) {
        tool::Logging(LOG_ERROR, myName, "Reading from file failed: general error(%s)\n", ex.what());
    }
}

void ServerEndpoint::BackupTrainTensors2Storage() {
    const char* myName = "BackupTraintensors";
    size_t miniBatchSize = 0;
    for (const auto& tensor : trainTensors_) {
        miniBatchSize += tensor.size;
    }
    if (trainTensorsMappingRegion_ == nullptr) {
        std::string fileName = backupFilePath_ + ".tmp";
        const char* fileNamecStr = fileName.c_str();
        std::ofstream ofs(fileName, std::ios::binary | std::ios::out | std::ios::trunc);
        if (!ofs) {
            throw std::runtime_error("Opening file failed: " + fileName);
        }
        ofs.seekp(miniBatchSize * BACKUP_PERIOD - 1);
        ofs.write("", 1);
        ofs.close();
        tool::Logging(LOG_DEBUG, myName, "File(%s) is resized to %zu bytes\n", fileNamecStr, miniBatchSize * BACKUP_PERIOD);
        
        /* open the file and map to memory */
        boost::interprocess::file_mapping fileMapping(fileNamecStr, boost::interprocess::read_write);
        trainTensorsMappingRegion_ = std::make_unique<boost::interprocess::mapped_region>(fileMapping, boost::interprocess::read_write, 0, miniBatchSize * BACKUP_PERIOD);
    }

    while (true) {
        if (!trainTensorBackup_.Check()) {
            break;
        }

        tool::Logging(LOG_DEBUG, myName, "start to backup the train tensors for iteration#%zu\n", curIter_);
    
        auto start = std::chrono::high_resolution_clock::now();
        size_t offset = (curIter_ % BACKUP_PERIOD) * miniBatchSize;
        for (size_t i = 0; i < trainTensors_.size(); i++) {
            int streamIdx = i % streamList_.size();
            // copy the data to the memory-mapped file
            cudaError_t exit_code = cudaMemcpyAsync((uint8_t*)trainTensorsMappingRegion_->get_address() + offset, GetDevPtr((uint64_t)trainTensors_[i].devPtr), trainTensors_[i].size, cudaMemcpyDeviceToHost, streamList_[streamIdx]); // backup in the non-blocking stream
            if (exit_code != cudaSuccess) {
                tool::Logging(LOG_ERROR, myName, "cudaMemcpyAsync failed for the memory segment in tensor[%zu] (%p) with %zu bytes: %s\n", i, trainTensors_[i].devPtr, trainTensors_[i].size, cudaGetErrorString(exit_code));
                exit(EXIT_FAILURE);
            }
            else {
                tool::Logging(LOG_DEBUG, myName, "backup the memory segment in tensor[%zu] (%p) with %zu bytes\n", i, trainTensors_[i].devPtr, trainTensors_[i].size);

                cudaError_t sync_code = cudaStreamSynchronize(streamList_[streamIdx]);
                if (sync_code != cudaSuccess) {
                    tool::Logging(LOG_ERROR, myName, "cudaStreamSynchronize failed: %s\n", cudaGetErrorString(sync_code));
                    exit(EXIT_FAILURE);
                }
            }
            offset += trainTensors_[i].size;
        }
        auto end = std::chrono::high_resolution_clock::now();
        tool::Logging(LOG_DEBUG, myName, "backup the train tensors for iteration#%zu in %f seconds\n", curIter_, std::chrono::duration<double>(end - start).count());

        trainTensorBackup_.Notify();
    }
}

void ServerEndpoint::LoadFromStorage(size_t iter) {
    const char* myName = "LoadTrainTensors";
    size_t miniBatchSize = 0;
    for (const auto& tensor : trainTensors_) {
        miniBatchSize += tensor.size;
    } 
    if (trainTensorsMappingRegion_ == nullptr) {
        std::string fileName = backupFilePath_ + ".tmp";
        std::ifstream ofs(fileName, std::ios::binary | std::ios::ate);
        if (!ofs.is_open()) {
            throw std::runtime_error(std::string("Unable to open file: ") + fileName);
        }
        std::streamsize fileSize = ofs.tellg();
        if (fileSize < 0) {
            throw std::runtime_error("Unable to get file size");
        }
        ofs.close();
        size_t dataSize = static_cast<size_t>(fileSize);

        boost::interprocess::file_mapping fileMapping(fileName.c_str(), boost::interprocess::read_write);
        trainTensorsMappingRegion_ = std::make_unique<boost::interprocess::mapped_region>(fileMapping, boost::interprocess::read_write, 0, dataSize);
    }  
    size_t offset = (iter % BACKUP_PERIOD) * miniBatchSize;
    for (const auto& tensor : trainTensors_) {
        cudaError_t exit_code = cudaMemcpyAsync(GetDevPtr((uint64_t)tensor.devPtr), (uint8_t*)trainTensorsMappingRegion_->get_address() + offset, tensor.size, cudaMemcpyHostToDevice, defaultStream_); // loading in the default stream because it is in the critical path
        offset += tensor.size;

        cudaError_t sync_code = cudaStreamSynchronize(defaultStream_);
        if (sync_code != cudaSuccess) {
            tool::Logging(LOG_ERROR, myName, "cudaStreamSynchronize failed: %s\n", cudaGetErrorString(sync_code));
            exit(EXIT_FAILURE);
        }
    }
    tool::Logging(LOG_INFO, myName, "load the train tensors for iteration#%zu\n", iter);
}

void ServerEndpoint::StopBackup() {
    // // notify the sub thread to exit
    // {
    //     boost::unique_lock<boost::mutex> lock(backupSync_.mutex);
    //     bufferReady_ = false;
    //     bufferFinished_ = true;
    //     backupSync_.cv.notify_one();
    // }
    // backupStorageThread_->join();

    cudaFreeHost(serverState_.hostPtr);
    cudaFreeHost(backupMemcpyBuffer_);
    for (int i = 0; i < streamList_.size(); i++) {
        cudaStreamDestroy(streamList_[i]);
    }
}

void ServerEndpoint::CommEventMonitor() {
    while (true) {
        std::list<cudaEvent_t> tmpList;

        boost::unique_lock<boost::mutex> lock(eventWatchedSync_.mutex);
        if (eventWatchedSync_.cv.wait_for(lock, boost::chrono::seconds(COMM_EVENT_TIMEOUT),
                            [this] { return bufferFinished_;})) {
            tool::Logging(LOG_INFO, "CommEventMonitor", "stop watching events\n");
            break;
        }

        tmpList = watchedEventsList_;
        if (!notCompleteEventsList_.empty()) {
            tool::Logging(LOG_INFO, "CommEventMonitor", "watching %zu events (not complete in last round)\n", notCompleteEventsList_.size());
            for (const auto& elem : notCompleteEventsList_) {
                Handle_t* eventHandle = _cuInfoMap.handleManager->GetHandleInfoByRealAddr(elem);
                if (eventHandle == NULL || eventHandle->valid == false) {
                    continue;
                }

                cudaError_t exit_code = cudaEventQuery(elem);
                if (exit_code != cudaSuccess) {
                    tool::Logging(LOG_ERROR, "CommEventMonitor", "cudaEventQuery failed for event(%p): %s\n", (void*)elem, cudaGetErrorString(exit_code));
                    // ncclCommAbort(curComm); // trigger segmentation fault
                    std::terminate(); // as stucking in the callback function, so need to use this to exit

                    break;                    
                }
            }
            // Normally, notCompleteEventsList_ should be empty after the above loop
            notCompleteEventsList_.clear(); 
        }

        if (tmpList.empty()) {
            continue;
        }
        tool::Logging(LOG_INFO, "CommEventMonitor", "watching %zu events\n", tmpList.size());
        for (const auto& elem : tmpList) {
            watchedEventsList_.remove(elem);
            Handle_t* eventHandle = _cuInfoMap.handleManager->GetHandleInfoByRealAddr(elem);
            tool::Logging(LOG_DEBUG, "CommEventMonitor", "watching event(%p from %p)\n", (void*)elem, (void*)eventHandle);

            if (eventHandle == NULL || eventHandle->valid == false) {
                // cudaEventDestroy(elem); // destroyed by cudaEventDestroyHandle
                continue;
            }

            cudaError_t exit_code = cudaEventQuery(elem);
            if (exit_code == cudaSuccess) {
                // cudaEventDestroy(elem); // destroyed by cudaEventDestroyHandle
            }
            else {
                notCompleteEventsList_.push_back(elem);
            }
        }
    }
}

void ServerEndpoint::StopCommEventMonitor() {
    {
        boost::unique_lock<boost::mutex> lock(eventWatchedSync_.mutex);
        bufferFinished_ = true;
        eventWatchedSync_.cv.notify_one();
    }
    eventWatchedThread_->join();
}
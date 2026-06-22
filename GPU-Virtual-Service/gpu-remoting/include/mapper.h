#ifndef MAPPERS_H
#define MAPPERS_H

#include "configure.h"
#include "./hashing/robin_hood.h"
#include <cuda_runtime.h>
#include <nccl.h>

class DeviceBlockMapper {
private:
    const char* myName_ = "BlockMapper";
    std::vector<Block_t> blocks_; // !: start from 0
    uint64_t baseAddr_;

public:
    DeviceBlockMapper(const size_t reserveSize = 200, uint64_t base = 0x7f12e0a60000) {
        blocks_.reserve(reserveSize);
        baseAddr_ = base;
    }

    const std::vector<Block_t>& GetBlocks() const {
        return blocks_; // only read
    }

    std::vector<Block_t>& GetBlocks() {
        return blocks_; // read and write
    }

    void Resize(const size_t size) {
        blocks_.resize(size);
    }

    uint64_t AddBlock(const uint64_t devPtr, const size_t size, bool essential = false) {
        if (blocks_.empty()) {
            blocks_.emplace_back(Block_t{.start = baseAddr_, .devPtr = devPtr, .size = size, .valid = true, .essential = essential});
        }
        else {
            blocks_.emplace_back(Block_t{.start = blocks_.back().start + blocks_.back().size, .devPtr = devPtr, .size = size, .valid = true, .essential = essential});
        }
        // printf("AddBlock: devPtr = %p, blockIdx = %zu\n", (void*)devPtr, blocks_.size() - 1);
        return blocks_.back().start;
    }

    int FindByVirAddr(const uint64_t userAddr, uint64_t& devPtr) {
        auto it = std::lower_bound(blocks_.begin(), blocks_.end(), userAddr,
            [](const Block_t& block, uint64_t addr) { return block.start + block.size <= addr; });

        if (it != blocks_.end() && it->start <= userAddr && userAddr < it->start + it->size) {
            if (it->valid == false) {
                return -1;
            }
            else {
                devPtr = it->devPtr + userAddr - it->start;
                return it - blocks_.begin();
            }

        }
        return -1;
    }

    int FindByRealAddr(const uint64_t devPtr, uint64_t& userAddr) {
        for (size_t i = 0; i < blocks_.size(); i++) {
            if (blocks_[i].devPtr <= devPtr && devPtr < blocks_[i].devPtr + blocks_[i].size) {
                userAddr = blocks_[i].start + devPtr - blocks_[i].devPtr;
                return i;
            }
        }
        return -1;
    }


    bool ResetBlock(const uint64_t userAddr, int wantIdx = -1) {
        uint64_t devPtr = 0;
        int idx = (wantIdx == -1) ? FindByVirAddr(userAddr, devPtr) : wantIdx;
        if (idx != -1) {
            // printf("ResetBlock: devPtr = %p, blockIdx = %d\n", (void*)devPtr, idx);
            blocks_[idx].valid = false;
            blocks_[idx].devPtr = 0;
            return true;
        }
        else {
            return false;
        }
    }

    void Print() {
        tool::Logging(LOG_INFO, myName_, "BlockManager has %zu blocks\n", blocks_.size());
        for (size_t i = 0; i < blocks_.size(); i++) {
            tool::Logging(LOG_INFO, myName_, "Block[%zu]: start = %p, devPtr = %p, size = %zu, valid = %d, essential = %d\n", i, (void*)blocks_[i].start, (void*)blocks_[i].devPtr, blocks_[i].size, blocks_[i].valid, blocks_[i].essential);
        }
    }
};

class HandleMapper {
private:
    const char* myName_ = "HandleMapper";
    std::vector<Handle_t> handleInfoList; // !: start from 1
    robin_hood::unordered_flat_map<uint64_t, size_t> mapRealAddr2handleListIdx;
    std::queue<pair<size_t, size_t>> freeHandleQueue;

public:
    HandleMapper(const size_t reserveSize = 1000) {
        handleInfoList.reserve(reserveSize);
        handleInfoList.emplace_back(Handle_t{.handlePtr = (uint64_t)NULL, .type = __CUDA_REGISTER, .valid = true, .stream = 0});  // let the handleNum start from 1
    }

    const std::vector<Handle_t>& GetHandleInfoList() const {
        return handleInfoList; // only read
    }

    std::vector<Handle_t>& GetHandleInfoList() {
        return handleInfoList; // read and write
    }

    void Resize(const size_t size) {
        handleInfoList.resize(size);
    }

    void Reset(const std::vector<std::pair<size_t, Handle_t>>& handleList) {
        handleInfoList.resize(handleList.back().first + 1); // handleIdxList is sorted and has at least one element
        mapRealAddr2handleListIdx.clear();
        while (!freeHandleQueue.empty()) {
            freeHandleQueue.pop();
        }
        for (size_t i = 0; i < handleList.size(); i++) {
            size_t handleIdx = handleList[i].first;
            handleInfoList[handleIdx] = handleList[i].second;
            if (i == 0) {
                continue;
            }
            mapRealAddr2handleListIdx.insert({(uint64_t)handleInfoList[handleIdx].handlePtr, handleIdx});
            // no need to push the free space into the queue since the replaying process has not been completed
            // if (i == 1 && handleIdx != 1) {
            //     freeHandleQueue.push({1, handleIdx - 1});
            // }
            // else if (i > 1 && handleIdx != handleList[i - 1].first + 1) {
            //     freeHandleQueue.push({handleList[i - 1].first + 1, handleIdx - 1});
            // }
        }
    }

    void Shrink() {
        tool::Logging(LOG_DEBUG, myName_, "Shrink: handleInfoList size: %zu (mapRealAddr2handleListIdx size: %zu)\n", handleInfoList.size(), mapRealAddr2handleListIdx.size());
        mapRealAddr2handleListIdx.clear();
        while (!freeHandleQueue.empty()) {
            freeHandleQueue.pop();
        }

        for (size_t i = 1; i < handleInfoList.size(); i++) {
            if (handleInfoList[i - 1].valid == false && handleInfoList[i].valid == false) {
                freeHandleQueue.back().second = i;
            }
            else if (handleInfoList[i - 1].valid == true && handleInfoList[i].valid == false) {
                freeHandleQueue.push({i, i});
            }

            if (handleInfoList[i].valid == true) {
                mapRealAddr2handleListIdx.insert({(uint64_t)handleInfoList[i].handlePtr, i});
            }
        }
        tool::Logging(LOG_DEBUG, myName_, "Shrink: mapRealAddr2handleListIdx (valid handles) is reorganized to %zu\n", mapRealAddr2handleListIdx.size());        
    }

    void Indexing() {
        mapRealAddr2handleListIdx.clear();
        for (size_t i = 1; i < handleInfoList.size(); i++) {
            if (handleInfoList[i].valid == true) {
                mapRealAddr2handleListIdx.insert({(uint64_t)handleInfoList[i].handlePtr, i});
            }
        }
    }

    void* AddHandle(void* realAddr, enum API_REQUEST_CODE_SET handleType) {
        auto it = mapRealAddr2handleListIdx.find((uint64_t)realAddr);
        size_t handleNum = 0;
        if (it != mapRealAddr2handleListIdx.end()) {
            handleNum = it->second; // find out the address previously created
            handleInfoList[handleNum].valid = true;
            handleInfoList[handleNum].type = handleType;
        }
        else {
            if (freeHandleQueue.empty()) { // handleInfo List has no free space to add a new handle, so need to expand
                handleNum = (
                    handleInfoList.emplace_back(Handle_t{.handlePtr = (uint64_t)realAddr, .type = handleType, .valid = true, .stream = 0}),
                    handleInfoList.size() - 1
                );
            }
            else { // reuse the free space in handleInfo List
                handleNum = freeHandleQueue.front().first;
                freeHandleQueue.front().first++;
                if (freeHandleQueue.front().first > freeHandleQueue.front().second) {
                    freeHandleQueue.pop();
                }
                handleInfoList[handleNum].handlePtr = (uint64_t)realAddr;
                handleInfoList[handleNum].type = handleType;
                handleInfoList[handleNum].valid = true;
                handleInfoList[handleNum].stream = 0;
            }
            mapRealAddr2handleListIdx.insert({(uint64_t)realAddr, handleNum});               

            if (handleInfoList.size() >= HANDLE_MAX_NUM && freeHandleQueue.size() == 0) {
                tool::Logging(LOG_INFO, myName_, "AddHandle: mapRealAddr2handleListIdx is too large(%zu), preparing to reorganize\n", mapRealAddr2handleListIdx.size());
                Shrink();
            }
        }
        // no need to check the handleNum, since ((1LL << 48) - 1) is enough large 
        uint64_t handleVirAddr = HANDLE_PREFIX | handleNum;
        return (void*)handleVirAddr;
    }

    void* FindRealAddrByVirAddr(uint64_t userAddr, bool reset) {
        if (CHECK_HANDLE_PREFIX(userAddr) == 0) {
            return NULL;
        }
        size_t handleID = GET_HANDLE_ID(userAddr);
        if (handleID <= 0 || handleID >= handleInfoList.size() || handleInfoList[handleID].valid == false) {
            return NULL;
        }
        void* realAddr = (void*)(handleInfoList[handleID].handlePtr);
        if (reset) {
            handleInfoList[handleID].valid = false;
        }
        return realAddr;
    }

    Handle_t* GetHandleInfoByRealAddr(void* realAddr) {
        auto it = mapRealAddr2handleListIdx.find((uint64_t)realAddr);
        if (it != mapRealAddr2handleListIdx.end()) {
            return &handleInfoList[it->second];
        }
        else {
            // for (size_t i = 1; i < handleInfoList.size(); i++) {
            //     if (handleInfoList[i].handlePtr == (uint64_t)realAddr) {
            //         return &handleInfoList[i];
            //     }
            // }
            // tool::Logging(LOG_ERROR, myName_, "GetHandleInfoByRealAddr failed: realAddr(%p) is not in the map\n", realAddr);
            return NULL;
        }
    }

    uint64_t FindIdxByRealAddr(void* realAddr) {
        auto it = mapRealAddr2handleListIdx.find((uint64_t)realAddr);
        if (it != mapRealAddr2handleListIdx.end()) {
            uint64_t virtAddr = HANDLE_PREFIX | it->second;
            return virtAddr;
        }
        else {
            for (size_t i = 1; i < handleInfoList.size(); i++) {
                if (handleInfoList[i].handlePtr == (uint64_t)realAddr) {
                    return HANDLE_PREFIX | i;
                }
            }
            tool::Logging(LOG_ERROR, myName_, "FindIdxByRealAddr failed: realAddr(%p) is not in the map\n", realAddr);
            return 0;
        }            
        return 0;
    }

    Handle_t* GetHandleInfoByVirAddr(uint64_t userAddr) {
        if (CHECK_HANDLE_PREFIX(userAddr) == 0) {
            return NULL;
        }
        size_t handleID = GET_HANDLE_ID(userAddr);
        if (handleID <= 0 || handleID >= handleInfoList.size()) {
            return NULL;
        }
        return &handleInfoList[handleID]; //! the pointer will be changed
    }

    void UpdateHandle(const uint64_t userAddr, void* realAddr, enum API_REQUEST_CODE_SET handleType) {
        if (CHECK_HANDLE_PREFIX(userAddr) == 0) {
            tool::Logging(LOG_ERROR, myName_, "UpdateHandle failed: userAddr is not a handle(%p)\n", (void*)userAddr);
            return;
        }
        size_t handleID = GET_HANDLE_ID(userAddr);
        if (handleID <= 0) {
            tool::Logging(LOG_ERROR, myName_, "UpdateHandle failed: handleID(%zu) is out of range\n", handleID);
            return;
        }
        if (handleID == handleInfoList.size()) {
            handleInfoList.emplace_back(Handle_t{.handlePtr = (uint64_t)realAddr, .type = handleType, .valid = true, .stream = 0});
        }
        if (handleID > handleInfoList.size()) {
            handleInfoList.resize(handleID + 1); // expand the boundary
            handleInfoList[handleID] = Handle_t{.handlePtr = (uint64_t)realAddr, .type = handleType, .valid = true, .stream = 0};
        } // todo: to be optimized

        // No need to update the mapRealAddr2handleListIdx, since shrink() will be called after the replaying process
        // auto it = mapRealAddr2handleListIdx.find((uint64_t)realAddr);
        // if (it != mapRealAddr2handleListIdx.end()) {
        //     it->second = handleID; // update the handleID of the realAddr
        // }
        // else {
        //     mapRealAddr2handleListIdx.insert({(uint64_t)realAddr, handleID});
        // } // this handleID was created after the checkpoint, maybe was not in the map
        
        if (handleID < handleInfoList.size()) { // handleID is located in the existing range (not include the boundary)
            handleInfoList[handleID].handlePtr = (uint64_t)realAddr;
            handleInfoList[handleID].valid = true;
        } 

        tool::Logging(LOG_DEBUG, myName_, "UpdateHandle: userAddr=%p, original realAddr=%p, new realAddr=%p\n", (void*)userAddr, (void*)handleInfoList[handleID].handlePtr, realAddr);
    }

    size_t GetValidHandleNum() {
        size_t validHandleNum = 0;
        for (size_t i = 1; i < handleInfoList.size(); i++) {
            if (handleInfoList[i].valid == true) {
                validHandleNum++;
            }
        }
        return validHandleNum;
    }

    std::vector<std::pair<size_t, Handle_t>> GetValidHandles() {
        std::vector<std::pair<size_t, Handle_t>> validHandles;
        for (size_t i = 0; i < handleInfoList.size(); i++) {
            if (handleInfoList[i].valid == true) {
                validHandles.emplace_back(i, handleInfoList[i]);
            }
        }
        return validHandles;
    }

    size_t GetCapacity() {
        return handleInfoList.size();
    }   
};

#endif // MAPPERS_H
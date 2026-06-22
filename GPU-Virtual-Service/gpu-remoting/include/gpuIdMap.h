#ifndef GPU_ID_MAP_H
#define GPU_ID_MAP_H

#include <fcntl.h>
#include <sys/mman.h>
#include "chunkStructure.h"
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/iostreams/device/array.hpp>
#include <boost/iostreams/stream.hpp>
#include <sstream>
#include "define.h"
#include <cuda_runtime.h>

struct GpuInfoEntry_t {
    int nodeDevIdx;
    uint16_t nodePort;
    char nodeIp[IP_STRING_LEN];
    uint16_t dataPort;
    char dataIp[IP_STRING_LEN];
    cudaDeviceProp devprop;
};

class GPUidMap{
    private:
        const char* myName_ = "GPUidMap";
        int proxySock_ = 0;
        std::vector<GpuInfoEntry_t*> gpuInfoList_;
        uint64_t clientID_;
        size_t devNum_;
        size_t reqCommIDcnt_ = 1; // request NCCL unique ID count (avoid request for old ID, start from 1)
#ifdef GV_MSGHANDLER
        const char* model_;
        size_t batch_Size_;
#endif
    public: 
#ifndef GV_MSGHANDLER
        GPUidMap(size_t devNum, uint64_t clientID, const std::string& proxyIP, uint16_t proxyPort) {
            clientID_ = clientID;
            devNum_ = devNum;
            gpuInfoList_.reserve(devNum_);

            struct sockaddr_storage serv_addr;
            if ((proxySock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                tool::Logging(LOG_ERROR, myName_, "Socket creation failed\n");
                exit(EXIT_FAILURE);
            }

            tool::SetSockAddr(proxyIP.c_str(), proxyPort, &serv_addr, AF_INET);
            if (connect(proxySock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                tool::Logging(LOG_ERROR, myName_, "Connection to GPU Proxy(%s:%d) failed\n", proxyIP.c_str(), proxyPort);
                exit(EXIT_FAILURE);
            }
            else {
                tool::Logging(LOG_INFO, myName_, "Connection to GPU Proxy success\n");
            }

            RequestGPU();
        }
#else
        GPUidMap(size_t devNum, uint64_t clientID, const std::string& model, size_t batch_Size, const std::string& proxyIP, uint16_t proxyPort) {
            clientID_ = clientID;
            devNum_ = devNum;
            model_ = model.c_str();
            batch_Size_ = batch_Size;
            gpuInfoList_.reserve(devNum_);

            struct sockaddr_storage serv_addr;
            if ((proxySock_ = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                tool::Logging(LOG_ERROR, myName_, "Socket creation failed\n");
                exit(EXIT_FAILURE);
            }

            tool::SetSockAddr(proxyIP.c_str(), proxyPort, &serv_addr, AF_INET);
            if (connect(proxySock_, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                tool::Logging(LOG_ERROR, myName_, "Connection to GPU Proxy(%s:%d) failed\n", proxyIP.c_str(), proxyPort);
                exit(EXIT_FAILURE);
            }
            else {
                tool::Logging(LOG_INFO, myName_, "Connection to GPU Proxy success\n");
            }

            RequestGPU();
        }
#endif
        ~GPUidMap() {
            tool::Logging(LOG_DEBUG, myName_, "Close socket to GPU Proxy\n");
            close(proxySock_);
        }

        void RequestGPU() {
#ifndef GV_MSGHANDLER
            std::string sendData = "GPUQuery:" + std::to_string(clientID_) + "," + std::to_string(devNum_);
#else
            std::string sendData = "GPUQuery:" + std::to_string(clientID_) + "," + std::to_string(devNum_) + "," + model_ + "," + std::to_string(batch_Size_);
#endif
            // 
            size_t sendDataLen = sendData.size();
            send(proxySock_, &sendDataLen, sizeof(size_t), 0);
            send(proxySock_, sendData.c_str(), sendDataLen, 0);
            tool::Logging(LOG_DEBUG, myName_, "Send GPU allocation request(%zu bytes) to GPU Proxy\n", sendDataLen);

            for (int i = 0; i < devNum_; i++) {
                GpuInfoEntry_t* gpuInfo = (GpuInfoEntry_t*)malloc(sizeof(GpuInfoEntry_t));
                if (!tool::ReadSocketMessage(proxySock_, (uint8_t*)gpuInfo, sizeof(GpuInfoEntry_t))) {
                    // printf("Failed to read buffer length from GPU Proxy\n");
                    tool::Logging(LOG_ERROR, myName_, "Failed to recv gpu info list from GPU Proxy\n");
                    exit(EXIT_FAILURE);
                }
                else {
                    tool::Logging(LOG_INFO, myName_, "GPU ID: %d, IP Address: %s, Port: %d, DataIP: %s, DataPort: %d, prop->name: %s\n", gpuInfo->nodeDevIdx, gpuInfo->nodeIp, gpuInfo->nodePort, gpuInfo->dataIp, gpuInfo->dataPort, gpuInfo->devprop.name);
                }
                gpuInfoList_.emplace_back(gpuInfo);
            }
        }

        void ReallocGPU(){
            tool::Logging(LOG_DEBUG, myName_, "ReallocGPU: ready to realloc GPU\n");
            std::string sendData = "GPURealloc:" + std::to_string(clientID_) + "," + std::to_string(devNum_);
            size_t sendDataLen = sendData.size();
            send(proxySock_, &sendDataLen, sizeof(size_t), 0);
            send(proxySock_, sendData.c_str(), sendDataLen, 0);
            tool::Logging(LOG_DEBUG, myName_, "Send GPU Reallocation request(%zu bytes) to GPU Proxy\n", sendDataLen);
            for(int i = 0; i < devNum_; ++i){
                tool::Logging(LOG_INFO, myName_, "Waiting for GPU reallocation\n");
                GpuInfoEntry_t* gpuInfo = (GpuInfoEntry_t*)malloc(sizeof(GpuInfoEntry_t));
                if (!tool::ReadSocketMessage(proxySock_, (uint8_t*)gpuInfo, sizeof(GpuInfoEntry_t))) {//没得到GPU前阻塞
                    tool::Logging(LOG_ERROR, myName_, "Failed to recv gpu info list from GPU Proxy\n");
                    exit(EXIT_FAILURE);
                }
                else {
                    tool::Logging(LOG_INFO, myName_, "Reallocate GPU ID: %d, IP Address: %s, Port: %d, DataIP: %s, DataPort: %d, prop->name: %s\n", gpuInfo->nodeDevIdx, gpuInfo->nodeIp, gpuInfo->nodePort, gpuInfo->dataIp, gpuInfo->dataPort, gpuInfo->devprop.name);
                }
                gpuInfoList_[i] = gpuInfo;
            }
            // gpuInfoList_[0]->nodeDevIdx = 1;

        }   

        void UpdateUniqueID(const uint8_t* uniqueID, const size_t len) {
            std::string sendData = "CommUpdate:" + std::to_string(clientID_) + "," + std::to_string(len);
            size_t sendDataLen = sendData.size();
            send(proxySock_, &sendDataLen, sizeof(size_t), 0);
            send(proxySock_, sendData.c_str(), sendDataLen, 0);
            send(proxySock_, uniqueID, len, 0);
            tool::Logging(LOG_DEBUG, myName_, "Send NCCL unique ID update request(%zu bytes) to GPU Proxy\n", sendDataLen);
            // for (size_t i = 0; i < len; i++) {
            //     printf("%02x", uniqueID[i]);
            // }
            // printf("\n");
        }

        void RequestUniqueID(uint8_t* uniqueID, const size_t len) {
            reqCommIDcnt_++;
            uint8_t isVaild = false;
            std::string sendData = "CommQuery:" + std::to_string(clientID_) + "," + std::to_string(reqCommIDcnt_);
            size_t sendDataLen = sendData.size();
            while (!(bool)isVaild) {
                send(proxySock_, &sendDataLen, sizeof(size_t), 0);
                send(proxySock_, sendData.c_str(), sendDataLen, 0);
                tool::Logging(LOG_DEBUG, myName_, "Send NCCL unique ID request to GPU Proxy, waiting for response...\n");
                if (!tool::ReadSocketMessage(proxySock_, &isVaild, sizeof(uint8_t))) {
                    tool::Logging(LOG_ERROR, myName_, "Failed to recv NCCL unique ID from GPU Proxy\n");
                    exit(EXIT_FAILURE);
                }
            }

            if (!tool::ReadSocketMessage(proxySock_, uniqueID, len)) {
                tool::Logging(LOG_ERROR, myName_, "Failed to recv NCCL unique ID from GPU Proxy\n");
                exit(EXIT_FAILURE);
            }
            else {
                tool::Logging(LOG_DEBUG, myName_, "Received NCCL unique ID from GPU Proxy\n");
            }
            // for (size_t i = 0; i < len; i++) {
            //     printf("%02x", uniqueID[i]);
            // }
            // printf("\n");
        }

        void Print() {
            tool::Logging(LOG_INFO, myName_, "GPU ID Map:\n");
            for (int i = 0; i < gpuInfoList_.size(); i++) {
                tool::Logging(LOG_INFO, myName_, "[%d] GPU ID: %d, IP Address: %s, Port: %d\n", i, gpuInfoList_[i]->nodeDevIdx, gpuInfoList_[i]->nodeIp, gpuInfoList_[i]->nodePort);
            }
        }


        inline bool Check(int virtDevIdx) {
            if (virtDevIdx < 0 || virtDevIdx >= gpuInfoList_.size()) {
                tool::Logging(LOG_ERROR, myName_, "Invalid virtual device index: %d (max: %d)\n", virtDevIdx, gpuInfoList_.size());
                return false;
            }
            return true;
        }

        bool GetGPUKey(int gpuIdxInNode, int* virtDevIdx) {
            tool::Logging(LOG_DEBUG, myName_, "GPUIdGetKey: ready to read the key(gpuIdx=%d) from GPU ID Map\n", gpuIdxInNode);
            for (int i = 0; i < gpuInfoList_.size(); i++) {
                if (gpuInfoList_[i]->nodeDevIdx == gpuIdxInNode) {
                    *virtDevIdx = i;
                    return true;
                }
            }
            return false;
        }

        bool GetGPUId(int virtDevIdx, int* gpuIdxInNode) {
            tool::Logging(LOG_DEBUG, myName_, "GPUIdGetValue: ready read the key(virDev=%d) from GPU ID Map\n", virtDevIdx);
            Check(virtDevIdx);
            *gpuIdxInNode = gpuInfoList_[virtDevIdx]->nodeDevIdx;
            return true;
        }

        bool GetGPUprop(int virtDevIdx, cudaDeviceProp* prop) {
            tool::Logging(LOG_DEBUG, myName_, "GetGpuIdMap: ready to read the key(virDev=%d) from GPU ID Map\n", virtDevIdx);
            Check(virtDevIdx);
            *prop = gpuInfoList_[virtDevIdx]->devprop;
            return true;
        }

        bool GetGPUinfo(int virtDevIdx, GpuInfoEntry_t** gpuInfo) {
            tool::Logging(LOG_DEBUG, myName_, "GetGpuIdMap: ready to read the key(virDev=%d) from GPU ID Map\n", virtDevIdx);
            Check(virtDevIdx);
            *gpuInfo = gpuInfoList_[virtDevIdx];
            return true;
        }

        bool SetGPUinfo(int virtDevIdx, const GpuInfoEntry_t& gpuInfo) {
            tool::Logging(LOG_DEBUG, myName_, "SetGpuIdMap: ready to write the key(virDev=%d) to GPU ID Map\n", virtDevIdx);
            Check(virtDevIdx);
            memcpy(gpuInfoList_[virtDevIdx], &gpuInfo, sizeof(GpuInfoEntry_t));
            return true;
        }

};


#endif
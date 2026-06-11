#ifndef CLIENT_ENDPOINT_H
#define CLIENT_ENDPOINT_H

#include "configure.h"
#include "shmqueue/shmUtil.h"
#include "ucpConnection.h"
#include "requestBuffer.h"
#include "requestIOV.h"
#include "registerIOV.h"
#include "gpuIdMap.h"
#include "./hashing/robin_hood.h"
#include "./conqueue/readerwriterqueue.h"

class SpinLock {
private:
    std::atomic_flag flag = ATOMIC_FLAG_INIT;

public:
    void lock() {
        while (flag.test_and_set(std::memory_order_acquire)) {
            // Busy wait
        }
    }

    void unlock() {
        flag.clear(std::memory_order_release);
    }
};

class ClientEndpoint {
    private:
        const char* myName_ = "ClientEndpoint";
        char* clientIP_;
        char* clientPort_;
        size_t priority_ = 0;

        ConnStatus_t connStatus_ = {false, true};

        vector<double> clearTimes_;

        RequestIOV* recordedReq = NULL;
        SpinLock listLock_;
        bool memcpyRecord_ = false;
        boost::intrusive::list<RequestIOV> reqIOVList;
        // int ckptIter = 0;
        // int ckptCnt = 0;
        
    public:
        uint64_t _clientID;
        int _myDevIdx = 0;
        int _threadID = 0;
        int _processID = 0;

        ucp_worker_h _dataWorker;
        ucp_ep_h _serverEp;

        size_t _copySize = 0;  // data size that has been copied
        SharedMemoryOpt* _shmOpt = NULL;
        SharedMemoryOpt* _GpuIdMap = NULL;

        ClientEndpoint(uint64_t clientID, size_t priority, ucp_worker_h clientWorker, int dev);

        ~ClientEndpoint();

        void Connect(bool replay = false);
        void CloseEp(uint64_t flags);

        static void SendRegisterRequest(ClientEndpoint* curEp, bool forcedEager = true);
        ucs_status_t SendRequest(RequestIOV* reqBuffer, bool forcedEager = false, bool isCheckpoint = true);
        void SendRequestH2D(RequestIOV* reqBuffer, uint8_t* header, size_t headerSize, bool forcedEager = false, bool isCheckpoint = true);
        ucs_status_t SendRequestRecvResponse(RequestIOV* reqBuffer, RequestIOV* responseBuffer, bool forcedEager = false, bool isCheckpoint = true);
        void SendNewIterRequest(size_t iterNum);
        
        void Checkpointing();
        void Replay();
        void UpdateReqIOVList(RequestIOV* reqBuffer);
        void UpdateReqIOVList(RequestIOV* reqBuffer, uint8_t* header, size_t headerSize);
        void ShrinkReqIOVList();

        void SendMainDevice(bool replay);
};

extern std::mutex reConnectMutex;
extern bool isReConnected;
extern thread_local int ttID;
extern SharedMemoryOpt* shmOpt;
extern std::vector<RegisterIOV*> regIOVList;
extern GPUidMap* gpuIdMap;
extern Configure* config_;
extern thread_local std::vector<TensorInfo_t> tensorByteList;
extern void CheckTensors(int reqType);
extern ucs_status_t ServerStatusCallback(void *arg, const void *header, size_t header_length, void *data, size_t length, const ucp_am_recv_param_t *param);
extern void ClientErrorCallback(void *arg, ucp_ep_h ep, ucs_status_t status);

#endif
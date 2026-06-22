#ifndef SERVER_ENDPOINT_H
#define SERVER_ENDPOINT_H
#include <alloca.h>
#include "configure.h"
#include "requestBuffer.h"
#include "requestIOV.h"
#include "registerIOV.h"
#include "ucpConnection.h"
// #include "./tsl/bhopscotch_map.h"
#include "./hashing/robin_hood.h"
#include "./conqueue/readerwriterqueue.h"
#include "./shmqueue/shmUtil.h"
#include "ptxExtractor.h"
#include "mapper.h"
#include "asyncRequest.h"
#include <cuda.h>
#include <cuda_runtime.h>
#include <cublas_v2.h>
#include <cublasLt.h>
#include <cudnn.h>
#include <nccl.h>
// #include <cufile.h>
#include "scheduler.h"
#include "elasticscheduler.h"
#ifdef GV_MSGHANDLER
#include "msghandler.h"
#endif


typedef struct {
    DeviceBlockMapper* blockManager;
    std::vector<Block_t> blockInfoList;
    HandleMapper* handleManager;
    // robin_hood::unordered_flat_map<uint64_t, Handle_t*> mapVirAddr2HandleInfo;
    // robin_hood::unordered_flat_map<uint64_t, size_t> mapRealAddr2handleListIdx;
    // std::vector<Handle_t> handleInfoList;
    // std::queue<pair<size_t, size_t>> freeHandleQueue;
    std::vector<ncclRedOp_t> ncclRedOpList;
    // std::vector<ncclUniqueId> ncclUidList;

    bool isFirstIter;
    robin_hood::unordered_flat_map<uint64_t, CUmodule>* mapFatBinHandle2CuModule;
    robin_hood::unordered_flat_map<uint64_t, CUfunction>* mapHost2CuFunc;          //used for cudaLaunchKernel
    robin_hood::unordered_flat_map<uint64_t, CUdeviceptr>* mapHostVar2CuDevPtr;    //used for cudaMemcopyToSymbol
    robin_hood::unordered_flat_map<std::string, uint64_t>* mapDevName2DevPtr;    //used for identifying the device pointer, shared by all devices
    PTXExtractor* ptxExtractor;
    moodycamel::BlockingReaderWriterQueue< std::pair<void*, size_t> >* imageQueue;
} CUinfoMap_t;

#define DEFINE_SERVER_AM_CALLBACK(name) \
    ucs_status_t ServerEndpoint::name(void *arg, const void *header, size_t header_length, \
                             void *data, size_t length, const ucp_am_recv_param_t *param)


extern Configure config;
class ServerEndpoint {
    private:
        const char* myShortName_ = "ServerEp";
        char* myName_ = NULL;
        ConnStatus_t connStatus_ = {true, true};
        uint64_t clientID_;
        char* clientIP_;
        char* clientPort_;
        int clientPID_ = 0;
        size_t priority_ = 0;

#ifdef GV_Scheduler
        Scheduler* sche;
#endif

#ifdef GV_MSGHANDLER
        MsgHandler* msghandler;
        boost::thread* MSGRecvThread_ = nullptr;
        bool MSGStopFlag_ = false;
        bool createThreadFlag = false;
        int msgResetCnt = 0;
        bool msgWaitFlag_ = false;
#endif

#ifdef GV_eScheduler
        eScheduler* esche;
#endif

        bool initFlag_ = false; // to initialize the shared memory (and imageQueue) only once
        size_t curIter_ = 0;
        size_t numIterations = 0; 
        int ckptCnt = 0;

        int curDev_ = 0; // the current device of this thread
        bool recoveryFlag_ = false;
        bool replayFlag_ = false;
        int getDeviceFlag = 0;
        int CliDev_ = -1;

        SharedMemoryOpt* shmQueSizes_[3] = {NULL, NULL, NULL};
        CMessageQueue* shmQueues_[3] = {NULL, NULL, NULL};
        size_t lastCopyLen_[3] = {0, 0, 0};
        uint8_t lastCopyType_ = MEMCPY_OTHER;
        std::vector<TensorInfo_t> trainTensors_;

        ncclComm_t curComm = NULL; // todo: maybe not only one stream
        cudaStream_t commStream_ = NULL; 
        cudaStream_t defaultStream_ = NULL;

        std::list<cudaEvent_t> watchedEventsList_;
        std::list<cudaEvent_t> notCompleteEventsList_;
        Sync_t eventWatchedSync_;
        boost::thread* eventWatchedThread_ = nullptr;
        
        std::string backupFilePath_;
        HostBuffer_t serverState_ = {NULL, 0};
        void* backupMemcpyBuffer_ = NULL;
        std::vector<cudaStream_t> streamList_;
        Sync_t backupSync_;
        AsyncRequest stateBackup_;
        AsyncRequest trainTensorBackup_;
        bool bufferReady_ = false;
        bool bufferFinished_ = false;
        bool bufferResized_ = false;
        // boost::thread* backupStorageThread_ = nullptr;
        std::unique_ptr<boost::interprocess::mapped_region> fileMappingRegion_;
        std::unique_ptr<boost::interprocess::mapped_region> trainTensorsMappingRegion_ = nullptr;

    public:
        ucp_conn_request_h _connectReq;
        ucp_worker_h _dataWorker;
        ucp_ep_h _clientEp;

        CUinfoMap_t _cuInfoMap; 
        boost::thread* _ptxThread = nullptr;
        std::vector<void*> _fatbinList;

        static void CreateServerEp(ucp_conn_request_h conn_request, void *arg);
        ServerEndpoint(ucp_worker_h dataWorker, UCPConnection_t conn);
        ~ServerEndpoint();
        void CloseEp(uint64_t flags);

        void SendResponse(RequestIOV* reqBuffer, const ucp_ep_h* ep, ucs_memory_type_t memType = UCS_MEMORY_TYPE_HOST);
        void SendStatus(int status);

        // Get the device pointer from userAddr(virtAddr)
        inline void* GetDevPtr(uint64_t userAddr) {
#ifdef GV_MEMORY
            uint64_t realAddr = 0;
            if (_cuInfoMap.blockManager->FindByVirAddr(userAddr, realAddr) == -1) {
                return NULL;
            }
            else {
                return (void*)realAddr;
            }
#elif defined(GV_MEMORY_PTX)
            return (void*)(_cuInfoMap.blockInfoList[GET_BLOCK_ID(userAddr)].devPtr + GET_BLOCK_INTER_OFFSET(userAddr));
#else
            return (void*)userAddr;
#endif // GV_MEMORY
        }

        // Get the block index of current device's blockList from userAddr(virtAddr)
        inline size_t GetBlockIdx(uint64_t userAddr) {
            return GET_BLOCK_ID(userAddr);
        }

        // Find the virtual address of the device pointer from the real address
        inline uint64_t FindDevPtrVirAddr(void* devPtr) {
#ifdef GV_MEMORY
            uint64_t virtAddr = 0;
            if (_cuInfoMap.blockManager->FindByRealAddr((const uint64_t)devPtr, virtAddr) == -1) {
                tool::Logging(LOG_ERROR, myName_, "FindDevPtrVirAddr failed: devPtr(%p) not found\n", devPtr);
                return 0;
            }
            else {
                return virtAddr;
            }
#else
            return (uint64_t)devPtr;
#endif // GV_MEMORY
        }

        // Get the real address of the handle from userAddr(virtAddr)
        inline void* GetHandle(uint64_t userAddr, bool reset = false) {
#ifdef GV_HANDLE
            return _cuInfoMap.handleManager->FindRealAddrByVirAddr(userAddr, reset);
#else
            return (void*)(userAddr);
#endif // GV_HANDLE
        }

        inline Handle_t* GetHandleInfo(uint64_t userAddr) {
            return _cuInfoMap.handleManager->GetHandleInfoByVirAddr(userAddr);
        }

        // Set the handle info 
        inline void SetHandleVirAddr(uint64_t userAddr, void* handlePtr, enum API_REQUEST_CODE_SET handleType = __CUDA_REGISTER) {
            _cuInfoMap.handleManager->UpdateHandle(userAddr, handlePtr, handleType);
        }

        // Add the handle info and return the virtual address of the handle
        inline void* GetHandleVirAddr(void* handlePtr, enum API_REQUEST_CODE_SET handleType) {
#ifdef GV_HANDLE
            return _cuInfoMap.handleManager->AddHandle(handlePtr, handleType);
#else
            return handlePtr;
#endif // GV_HANDLE
        }

        // Find the virtual address of the handle from the real address
        inline uint64_t FindHandleVirAddr(void* handlePtr) {
#ifdef GV_HANDLE
            return _cuInfoMap.handleManager->FindIdxByRealAddr(handlePtr);
#else
            return (uint64_t)handlePtr;
#endif // GV_HANDLE
        }

        inline ncclRedOp_t GetNcclRedOp(ncclRedOp_t userOp, bool reset = false) {
#ifdef GV_HANDLE
            ncclRedOp_t redOp = _cuInfoMap.ncclRedOpList[userOp];
            if (reset) {
                _cuInfoMap.ncclRedOpList[userOp] = static_cast<ncclRedOp_t>(0);
            }
            return redOp;
#else
            return userOp;
#endif // GV_HANDLE
        }

        inline ncclRedOp_t GetNcclRedOpVirAddr(ncclRedOp_t redOp) {
#ifdef GV_HANDLE
            size_t redOpIndex = (
                _cuInfoMap.ncclRedOpList.emplace_back(redOp),
                _cuInfoMap.ncclRedOpList.size() - 1
            );
            return (ncclRedOp_t)redOpIndex;
#else
            return redOp;
#endif // GV_HANDLE
        }

        inline void SetNcclRedOpVirAddr(ncclRedOp_t redOp, ncclRedOp_t userOp) {
#ifdef GV_HANDLE
            _cuInfoMap.ncclRedOpList[userOp] = redOp;
#endif // GV_HANDLE
        }

        void Run();
        void NewRun();
        void UpdateStream(cudaStream_t stream, bool isCommStream = true);
        void CommEventMonitor();
        void StopCommEventMonitor();
        void Backup2Memory();
        void Backup2Storage();
        void BackupTrainTensors2Storage();
        void Persist2File(const char* fileName, const uint8_t* data, size_t size);
        void LoadFromStorage();
        void LoadFromStorage(size_t iter);
        void StopBackup();

        /* CUDA Runtime Internal */
        DECLARE_AM_CALLBACK(__cudaRegisterHandle);
        DECLARE_AM_CALLBACK(__cudaRegisterFatBinaryHandle);
        DECLARE_AM_CALLBACK(__cudaRegisterFunctionHandle);
        DECLARE_AM_CALLBACK(__cudaRegisterVarHandle);
        void __cudaUnregisterFatBinaryHandle();

        /* CUDA Runtime Execution */
        DECLARE_AM_CALLBACK(cudaLaunchKernelHandle);
        DECLARE_AM_CALLBACK(cudaFuncGetAttributesHandle);

        /* CUDA Runtime Device */
        DECLARE_AM_CALLBACK(cudaGetDeviceHandle);
        DECLARE_AM_CALLBACK(cudaGetDeviceCountHandle);
        DECLARE_AM_CALLBACK(cudaGetDevicePropertiesHandle);
        DECLARE_AM_CALLBACK(cudaSetDeviceHandle);
        DECLARE_AM_CALLBACK(cudaSetMainDeviceHandle);
        DECLARE_AM_CALLBACK(cudaDeviceSynchronizeHandle);
        DECLARE_AM_CALLBACK(cudaDeviceGetAttributeHandle);

        /* CUDA Runtime Memory */
        DECLARE_AM_CALLBACK(cudaMallocHandle);
        DECLARE_AM_CALLBACK(cudaMemGetInfoHandle);
        DECLARE_AM_CALLBACK(cudaMemsetHandle);
        DECLARE_AM_CALLBACK(cudaMemsetAsyncHandle);
        DECLARE_AM_CALLBACK(cudaFreeHandle);
        DECLARE_AM_CALLBACK(cudaMemcpyH2DHandle);
        DECLARE_AM_CALLBACK(cudaMemcpyD2HHandle);
        DECLARE_AM_CALLBACK(cudaMemcpyD2DHandle);
        DECLARE_AM_CALLBACK(NewIterHandle);
        // DECLARE_AM_CALLBACK(cudaMemcpyHandle);
        // DECLARE_AM_CALLBACK(cudaMemcpyAsyncHandle); 
        DECLARE_AM_CALLBACK(cudaMemcpyAsyncH2DHandle);
        DECLARE_AM_CALLBACK(cudaMemcpyAsyncD2HHandle);
        DECLARE_AM_CALLBACK(cudaMemcpyAsyncD2DHandle);
        DECLARE_AM_CALLBACK(cudaMemcpyToSymbolHandle);

        /* CUDA Runtime Event */
        DECLARE_AM_CALLBACK(cudaEventCreateHandle);
        DECLARE_AM_CALLBACK(cudaEventCreateWithFlagsHandle);
        DECLARE_AM_CALLBACK(cudaEventRecordHandle);
        DECLARE_AM_CALLBACK(cudaEventQueryHandle);
        DECLARE_AM_CALLBACK(cudaEventDestroyHandle);
        DECLARE_AM_CALLBACK(cudaEventElapsedTimeHandle);

        /* CUDA Runtime Stream */
        DECLARE_AM_CALLBACK(cudaStreamCreateHandle);
        DECLARE_AM_CALLBACK(cudaStreamCreateWithFlagsHandle);
        DECLARE_AM_CALLBACK(cudaStreamCreateWithPriorityHandle);
        DECLARE_AM_CALLBACK(cudaStreamWaitEventHandle);
        DECLARE_AM_CALLBACK(cudaStreamSynchronizeHandle);
        DECLARE_AM_CALLBACK(cudaStreamDestroyHandle);
        DECLARE_AM_CALLBACK(cudaStreamIsCapturingHandle);
        DECLARE_AM_CALLBACK(cudaStreamGetCaptureInfoHandle);

        /* CUDA Runtime Other */
        DECLARE_AM_CALLBACK(cudaOccupancyMaxActiveBlocksPerMultiprocessorHandle);
        DECLARE_AM_CALLBACK(cudaOccupancyMaxActiveBlocksPerMultiprocessorWithFlagsHandle);

        /* cuBLAS */
        DECLARE_AM_CALLBACK(cublasCreate_v2Handle);
        DECLARE_AM_CALLBACK(cublasSgemm_v2Handle);
        DECLARE_AM_CALLBACK(cublasSgemmStridedBatchedHandle);
        DECLARE_AM_CALLBACK(cublasDestroy_v2Handle);
        DECLARE_AM_CALLBACK(cublasSetStream_v2Handle);
        DECLARE_AM_CALLBACK(cublasSetWorkspace_v2Handle);
        DECLARE_AM_CALLBACK(cublasSetMathModeHandle);
        DECLARE_AM_CALLBACK(cublasGetMathModeHandle);

        /* cuBLASLt */
        DECLARE_AM_CALLBACK(cublasLtCreateHandle);
        DECLARE_AM_CALLBACK(cublasLtDestroyHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulDescCreateHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulDescDestroyHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulDescSetAttributeHandle);
        DECLARE_AM_CALLBACK(cublasLtMatrixLayoutCreateHandle);
        DECLARE_AM_CALLBACK(cublasLtMatrixLayoutDestroyHandle);
        DECLARE_AM_CALLBACK(cublasLtMatrixLayoutSetAttributeHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulPreferenceCreateHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulPreferenceDestroyHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulPreferenceSetAttributeHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulAlgoGetHeuristicHandle);
        DECLARE_AM_CALLBACK(cublasLtMatmulHandle);

        /* cuDNN */
        DECLARE_AM_CALLBACK(cudnnCreateHandle);
        DECLARE_AM_CALLBACK(cudnnDestroyHandle);
        DECLARE_AM_CALLBACK(cudnnCreateTensorDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnDestroyTensorDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnGetTensorSizeInBytesHandle);
        DECLARE_AM_CALLBACK(cudnnSetTensor4dDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnSetTensorNdDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnSetTensorNdDescriptorExHandle);
        DECLARE_AM_CALLBACK(cudnnCreateTensorTransformDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnSetTensorTransformDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnDestroyTensorTransformDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnInitTransformDestHandle);
        DECLARE_AM_CALLBACK(cudnnTransformTensorExHandle);
        DECLARE_AM_CALLBACK(cudnnTransformFilterHandle);
        DECLARE_AM_CALLBACK(cudnnCreateFilterDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnSetFilterNdDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnDestroyFilterDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnGetFilterSizeInBytesHandle);
        DECLARE_AM_CALLBACK(cudnnGetFoldedConvBackwardDataDescriptorsHandle);
        DECLARE_AM_CALLBACK(cudnnSetStreamHandle);
        DECLARE_AM_CALLBACK(cudnnBatchNormalizationBackwardExHandle);
        DECLARE_AM_CALLBACK(cudnnBatchNormalizationForwardTrainingExHandle);
        DECLARE_AM_CALLBACK(cudnnBatchNormalizationForwardInferenceHandle);
        DECLARE_AM_CALLBACK(cudnnBackendCreateDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnBackendDestroyDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnBackendSetAttributeHandle);
        DECLARE_AM_CALLBACK(cudnnBackendGetAttributeHandle);
        DECLARE_AM_CALLBACK(cudnnBackendExecuteHandle);
        DECLARE_AM_CALLBACK(cudnnBackendFinalizeHandle);
        DECLARE_AM_CALLBACK(cudnnGetBatchNormalizationBackwardExWorkspaceSizeHandle);
        DECLARE_AM_CALLBACK(cudnnGetBatchNormalizationForwardTrainingExWorkspaceSizeHandle);
        DECLARE_AM_CALLBACK(cudnnGetBatchNormalizationTrainingExReserveSpaceSizeHandle);
        DECLARE_AM_CALLBACK(cudnnCreateConvolutionDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnDestroyConvolutionDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnSetConvolutionGroupCountHandle);
        DECLARE_AM_CALLBACK(cudnnSetConvolutionMathTypeHandle);
        DECLARE_AM_CALLBACK(cudnnSetConvolutionNdDescriptorHandle);
        DECLARE_AM_CALLBACK(cudnnSetConvolutionReorderTypeHandle);
        DECLARE_AM_CALLBACK(cudnnGetConvolutionForwardAlgorithm_v7Handle);
        DECLARE_AM_CALLBACK(cudnnGetConvolutionBackwardFilterAlgorithm_v7Handle);
        DECLARE_AM_CALLBACK(cudnnGetConvolutionBackwardDataAlgorithm_v7Handle);
        DECLARE_AM_CALLBACK(cudnnGetConvolutionForwardWorkspaceSizeHandle);
        DECLARE_AM_CALLBACK(cudnnConvolutionForwardHandle);
        DECLARE_AM_CALLBACK(cudnnGetConvolutionBackwardDataWorkspaceSizeHandle);
        DECLARE_AM_CALLBACK(cudnnConvolutionBackwardFilterHandle);
        DECLARE_AM_CALLBACK(cudnnGetConvolutionBackwardFilterWorkspaceSizeHandle);
        DECLARE_AM_CALLBACK(cudnnConvolutionBackwardDataHandle);

        /* nccl */
        DECLARE_AM_CALLBACK(ncclGroupStartHandle);
        DECLARE_AM_CALLBACK(ncclGroupEndHandle);
        DECLARE_AM_CALLBACK(ncclCommInitRankHandle);
        DECLARE_AM_CALLBACK(ncclCommDestroyHandle);
        DECLARE_AM_CALLBACK(ncclCommGetAsyncErrorHandle);
        DECLARE_AM_CALLBACK(ncclGetUniqueIdHandle);
        DECLARE_AM_CALLBACK(ncclGetVersionHandle);
        DECLARE_AM_CALLBACK(ncclAllReduceHandle);
        DECLARE_AM_CALLBACK(ncclReduceHandle);
        DECLARE_AM_CALLBACK(ncclReduceScatterHandle);
        DECLARE_AM_CALLBACK(ncclAllGatherHandle);
        DECLARE_AM_CALLBACK(ncclBroadcastHandle);
        DECLARE_AM_CALLBACK(ncclSendHandle);
        DECLARE_AM_CALLBACK(ncclRecvHandle);
        DECLARE_AM_CALLBACK(ncclCommCountHandle);
        DECLARE_AM_CALLBACK(ncclCommUserRankHandle);
        DECLARE_AM_CALLBACK(ncclCommCuDeviceHandle);
        DECLARE_AM_CALLBACK(ncclCommAbortHandle);
        DECLARE_AM_CALLBACK(ncclCommInitAllHandle);
        DECLARE_AM_CALLBACK(ncclCommInitRankConfigHandle);
        DECLARE_AM_CALLBACK(ncclCommSplitHandle);
        DECLARE_AM_CALLBACK(ncclCommFinalizeHandle);
        DECLARE_AM_CALLBACK(ncclCommRegisterHandle);
        DECLARE_AM_CALLBACK(ncclCommDeregisterHandle);
        DECLARE_AM_CALLBACK(ncclMemAllocHandle);
        DECLARE_AM_CALLBACK(ncclMemFreeHandle);
        DECLARE_AM_CALLBACK(ncclRedOpCreatePreMulSumHandle);
        DECLARE_AM_CALLBACK(ncclRedOpDestroyHandle);
};

#endif
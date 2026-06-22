#ifndef REQUEST_IOV_H
#define REQUEST_IOV_H

#include "constVar.h"
#include "configure.h"
#include "ucpUtil.h"

class RequestIOV : public boost::intrusive::list_base_hook<> {
    private:
        const char* myName_ = "RequestIOV";
        size_t headers_[PARAM_MAX_NUM];
        ucp_dt_iov_t iovs_[PARAM_MAX_NUM];
        int iovIdx_;
        int requestType_;
        int threadID_; 
        int popIdx_;
        size_t paramTotalSize_;

        void* dataBuffer_ = NULL;

    public:
        RequestIOV() : iovIdx_(-1), popIdx_(-1), threadID_(-1), paramTotalSize_(0) {}

        RequestIOV(int threadID) : RequestIOV() {
            threadID_ = threadID;
        }

        RequestIOV(const void *header, size_t header_length, void *data) : RequestIOV() {
            int iovNum = (header_length - sizeof(size_t)) / sizeof(size_t); // not including the threadID
            size_t offset = 0;
            for (int i = 0; i < iovNum; i++) {
                CheckFull();
                iovIdx_++;
                iovs_[iovIdx_].length = headers_[iovIdx_] = ((size_t*)header)[i];
                iovs_[iovIdx_].buffer = UCS_PTR_BYTE_OFFSET(data, offset);
                offset += iovs_[iovIdx_].length;
            }
            threadID_ = ((size_t*)header)[iovNum];
        }

        ~RequestIOV() {
            iovIdx_ = -1;
            if (dataBuffer_ != NULL) { // means that this object is cloned
                void* realData = ((ucp_dt_iov_t*)dataBuffer_)->buffer;
                // printf("Release RequestIOV for requestType=%d, realData=%p(size=%zu)\n", requestType_, realData, ((ucp_dt_iov_t*)dataBuffer_)->length);
                if (requestType_ == CUDA_MEMCPY_ASYNC_H2D) {
                    // printf("Release RequestIOV for requestType=CUDA_MEMCPY_ASYNC_H2D, realData=%p(size=%zu), iovs[0]=%p(buffer=%p)\n", realData, ((ucp_dt_iov_t*)dataBuffer_)->length, iovs_, iovs_[0].buffer);
                }
                if (realData != NULL) {
                    free(realData);
                    // realData = NULL;
                }
                // dataBuffer_ = NULL;
            }
        }

        void Reset() {
            iovIdx_ = -1;
        }

        int GetNum() {
            return iovIdx_ + 1;
        }

        ucp_dt_iov_t* GetIOVs() {
            return iovs_;
        }

        size_t* GetHeaders() {
            return headers_;
        }

        size_t GetHeaderSize() {
            return sizeof(size_t) * (iovIdx_ + 1 + (threadID_ == -1 ? 0 : 1));
        }

        size_t GetParamTotalSize() {
            return paramTotalSize_;
        }

        int GetRequestType() {
            return requestType_;
        }

        int GetThreadID() {
            return threadID_;
        }

        inline void CheckFull() {
            if (unlikely(iovIdx_ + 1 >= PARAM_MAX_NUM)) {
                tool::Logging(LOG_ERROR, myName_, "push failed: already full, iovIdx=%zu\n", iovIdx_);
                exit(EXIT_FAILURE);
            }
        }

        inline void CheckPopEnd() {
            if (unlikely(popIdx_ + 1 > iovIdx_)) {
                tool::Logging(LOG_ERROR, myName_, "pop failed: already pop to the end\n");
                exit(EXIT_FAILURE);
            }
        }

        void PushThreadID(int threadID) {
            CheckFull(); // check the headers is full or not
            headers_[iovIdx_ + 1] = threadID_ = threadID; // threadID is the last element of the headers
        }

        template <class T>
        void Push(T& item) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T);
            iovs_[iovIdx_].buffer = &item;
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        template <class T>
        void PushConst(const T& item) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T);
            iovs_[iovIdx_].buffer = (void*)&item;
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        template <class T>
        void Push64BitPointer(T& item) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(uint64_t);
            iovs_[iovIdx_].buffer = &item;
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        template <class T>
        void Push64BitPointer(const T& item) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(uint64_t);
            iovs_[iovIdx_].buffer = (void*)&item;
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        void PushVar(void* ptr, size_t size) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = size;
            iovs_[iovIdx_].buffer = ptr;
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        void PushRequestType(int reqType){
            requestType_ = reqType;
        }

        template <class T>
        void Push(T* item, size_t num = 1) {
            CheckFull();
            iovIdx_++;
            if (item == NULL || num == 0) {
                headers_[iovIdx_] = iovs_[iovIdx_].length = 0;
            } else {
                headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T) * num;
                iovs_[iovIdx_].buffer = item;
            }
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        template <class T>
        void PushConst(const T* item, size_t num = 1) {
            CheckFull();
            iovIdx_++;
            if (item == NULL || num == 0) {
                headers_[iovIdx_] = iovs_[iovIdx_].length = 0;
            } else {
                headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T) * num;
                iovs_[iovIdx_].buffer = (void*)item;
            }
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        void PushCString(const char *s) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = strlen(s) + 1; //! including the null-terminator
            iovs_[iovIdx_].buffer = (void*)s;
            paramTotalSize_ += iovs_[iovIdx_].length;
        }

        template <class T>
        T Pop() {
            CheckPopEnd();
            popIdx_++;
            return *(T*)iovs_[popIdx_].buffer;
        }

        template <class T>
        T* AssignAddr() {
            CheckPopEnd();
            popIdx_++;
            if (iovs_[popIdx_].length == 0) {
                return NULL;
            }
            else {
                return (T*)iovs_[popIdx_].buffer;
            }
        }

        template <class T>
        T* AssignAddrForAll() {
            return AssignAddr<T>();
        }

        char* AssignCString(){
            CheckPopEnd();
            popIdx_++;
            if (iovs_[popIdx_].length == 1) { // including the null-terminator
                return NULL;
            }
            else {
                return (char*)iovs_[popIdx_].buffer;
            }
        }

        void Print() {
            tool::Logging(LOG_INFO, myName_, "RequestIOV has %zu elements, popIdx=%d\n", iovIdx_ + 1, popIdx_);
            for (size_t i = 0; i <= iovIdx_; i++) {
                tool::Logging(LOG_INFO, myName_, "\t[%zu]: len=%zu, buffer=%p\n", i, iovs_[i].length, *(void**)iovs_[i].buffer);
            }
        }

        RequestIOV* Clone(uint8_t* H2Dheaders = NULL, size_t headerSize = 0) {
            RequestIOV* newReq = new RequestIOV();
            newReq->iovIdx_ = iovIdx_;
            newReq->requestType_ = requestType_;
            newReq->threadID_ = threadID_;
            newReq->popIdx_ = popIdx_;
            newReq->paramTotalSize_ = paramTotalSize_;
            newReq->dataBuffer_ = newReq->iovs_;

            if (H2Dheaders) {
                memcpy((uint8_t*)newReq->headers_, H2Dheaders, headerSize);
            //     newReq->iovs_[0].length = iovs_[0].length;
            //     newReq->iovs_[0].buffer = iovs_[0].buffer;
            // }
            // else if (requestType_ == CUDA_MEMCPY_ASYNC_H2D) {
            //     memcpy(newReq->headers_, headers_, 4+sizeof(size_t)+sizeof(uint64_t)+sizeof(uint8_t)+sizeof(uint64_t));
                void* dataBuffer = malloc(paramTotalSize_);
                size_t offset = 0;
                for (size_t i = 0; i <= iovIdx_; i++) {
                    newReq->iovs_[i].length = iovs_[i].length;
                    // newReq->iovs_[i].buffer = iovs_[i].buffer;
                    newReq->iovs_[i].buffer = UCS_PTR_BYTE_OFFSET(dataBuffer, offset);
                    memcpy(newReq->iovs_[i].buffer, iovs_[i].buffer, newReq->iovs_[i].length);
                    offset += newReq->iovs_[i].length;
                }
            }
            else {
                void* dataBuffer = malloc(paramTotalSize_);
                size_t offset = 0;
                for (size_t i = 0; i <= iovIdx_; i++) {
                    newReq->iovs_[i].length = newReq->headers_[i] = headers_[i];
                    newReq->iovs_[i].buffer = UCS_PTR_BYTE_OFFSET(dataBuffer, offset);
                    memcpy(newReq->iovs_[i].buffer, iovs_[i].buffer, newReq->iovs_[i].length);
                    offset += newReq->iovs_[i].length;
                }
                newReq->headers_[iovIdx_ + 1] = threadID_;
            }
            
            return newReq;
        }

        bool ContainsHandle(uint64_t* handleList, size_t handleCnt) {
            for (size_t i = 0; i <= iovIdx_; i++) {
                if (iovs_[i].length != sizeof(uint64_t) || *(void**)iovs_[i].buffer == NULL) {
                    continue;
                }
                for (size_t j = 0; j < handleCnt; j++) { // todo: may be optimized
                    if (*(uint64_t*)iovs_[i].buffer == handleList[j]) {
                        return true;
                    }
                }
            }
            return false;
        }

        uint64_t GetHandleByIndex(size_t idx) {
            if (idx > iovIdx_) {
                return 0;
            }
            if (iovs_[idx].length != sizeof(uint64_t) || *(void**)iovs_[idx].buffer == NULL) {
                return 0;
            }
            return *(uint64_t*)iovs_[idx].buffer;
        }

        bool SetElement(size_t idx, void* buffer, size_t length) {
            if (idx > iovIdx_) {
                return false;
            }
            iovs_[idx].buffer = buffer;
            headers_[idx] = iovs_[idx].length = length;
            return true;
        }
};

#endif
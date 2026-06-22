#ifndef REGISTER_IOV_H
#define REGISTER_IOV_H

#include "constVar.h"
#include "configure.h"
#include "ucpUtil.h"

typedef struct {
    size_t threadID;
    size_t reqNum;
    size_t iovNum;
} RegisterHeader_t;

class RegisterIOV {
    private:
        const char* myName_ = "RegisterIOV";
        size_t headers_[REG_PARAM_MAX_NUM];
        ucp_dt_iov_t iovs_[REG_PARAM_MAX_NUM];
        int iovIdx_;
        int requestType_;
        int popIdx_;

        bool recvFlag_;
        RegisterHeader_t ucpHeader_;

    public:
        RegisterIOV() : iovIdx_(0), popIdx_(0), recvFlag_(false) {
            PushRequestType(__CUDA_REGISTER);
            ucpHeader_.reqNum = 0;
            ucpHeader_.threadID = -1;
            ucpHeader_.iovNum = 0;
            iovs_[iovIdx_].length = 0;
            iovs_[iovIdx_].buffer = headers_;
        }

        RegisterIOV(int threadID) : RegisterIOV() {
            ucpHeader_.threadID = threadID;
        }

        RegisterIOV(const void *header, size_t header_length, void *data) : RegisterIOV() {
            recvFlag_ = true;
            ucpHeader_.threadID = ((size_t*)header)[0];
            ucpHeader_.reqNum = ((size_t*)header)[1];
            ucpHeader_.iovNum = ((size_t*)header)[2];
            if (unlikely(ucpHeader_.iovNum >= REG_PARAM_MAX_NUM)) {
                tool::Logging(LOG_ERROR, myName_, "push failed: already full, iovNum=%zu\n", ucpHeader_.iovNum);
                exit(EXIT_FAILURE);
            }

            size_t offset = sizeof(size_t) * (ucpHeader_.iovNum + 1); // headers contain iovNum+1 elements (data starts from iovIdx_ = 1)
            for (int i = 1; i <= ucpHeader_.iovNum; i++) {
                iovIdx_++; // start from iovIdx_ = 1
                headers_[iovIdx_] = *(size_t*)UCS_PTR_BYTE_OFFSET(data, sizeof(size_t) * iovIdx_);

                iovs_[iovIdx_].length = headers_[iovIdx_];
                iovs_[iovIdx_].buffer = UCS_PTR_BYTE_OFFSET(data, offset);
                offset += iovs_[iovIdx_].length;
            }
            tool::Logging(LOG_REGS, myName_, "recvFlag=%d, threadID=%d, reqNum=%d, iovNum=%d, iovIdx=%d\n", recvFlag_, ucpHeader_.threadID, ucpHeader_.reqNum, ucpHeader_.iovNum, iovIdx_);
        }

        ~RegisterIOV() {
            if (!recvFlag_) {
                for (int i = 1; i <= ucpHeader_.iovNum; i++) { // not include the first iov 
                    free(iovs_[i].buffer);
                }
            }
            iovIdx_ = 0;
        }

        void Reset() {
            iovIdx_ = 0;
        }

        int GetNum() {
            return iovIdx_ + 1;
        }

        ucp_dt_iov_t* GetIOVs() {
            return iovs_;
        }

        size_t* GetUcpHeaders() {
            return (size_t*)&ucpHeader_;
        }

        size_t GetUcpHeaderSize() {
            return sizeof(ucpHeader_);
        }

        void PushRequestType(int reqType){
            requestType_ = reqType;
        }

        int GetRequestType() {
            return requestType_;
        }

        int GetThreadID() {
            return ucpHeader_.threadID;
        }

        int GetRequestNum() {
            return ucpHeader_.reqNum;
        }

        void PushThreadID (int threadID) {
            ucpHeader_.threadID = threadID;
            ucpHeader_.iovNum = iovIdx_;
            iovs_[0].length = sizeof(size_t) * (ucpHeader_.iovNum + 1);
            tool::Logging(LOG_DEBUG, myName_, "recvFlag=%d, threadID=%d, reqNum=%d, iovNum=%d, iovIdx=%d\n", recvFlag_, ucpHeader_.threadID, ucpHeader_.reqNum, ucpHeader_.iovNum, iovIdx_);
         }

        void PushSubRequestType(int reqType){
            Push(reqType);
            ucpHeader_.reqNum++;
        }

        inline void CheckFull() {
            if (unlikely(iovIdx_ + 1 >= REG_PARAM_MAX_NUM)) {
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

        template <class T>
        void Push(const T& item) {
            CheckFull();
            iovIdx_++;
            headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T);
            iovs_[iovIdx_].buffer = malloc(iovs_[iovIdx_].length);
            memcpy(iovs_[iovIdx_].buffer, &item, iovs_[iovIdx_].length);
        }

        template <class T>
        void Push64BitPointer(T& item) {
            Push((uint64_t)item);
        }

        template <class T>
        void Push(T* item, size_t num = 1) {
            CheckFull();
            iovIdx_++;
            if (item == NULL || num == 0) {
                headers_[iovIdx_] = iovs_[iovIdx_].length = 0;
            } else {
                headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T) * num;
                iovs_[iovIdx_].buffer = malloc(iovs_[iovIdx_].length);
                memcpy(iovs_[iovIdx_].buffer, item, iovs_[iovIdx_].length);
            }
        }

        template <class T>
        void PushConst(const T* item, size_t num = 1) {
            CheckFull();
            iovIdx_++;
            if (item == NULL || num == 0) {
                headers_[iovIdx_] = iovs_[iovIdx_].length = 0;
            } else {
                headers_[iovIdx_] = iovs_[iovIdx_].length = sizeof(T) * num;
                iovs_[iovIdx_].buffer = malloc(iovs_[iovIdx_].length);
                memcpy(iovs_[iovIdx_].buffer, item, iovs_[iovIdx_].length);
            }
        }

        void PushCString(const char *s) {
            PushConst(s, strlen(s) + 1); //! including the null-terminator
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
            tool::Logging(myName_, "%s has %zu elements, popIdx=%d\n", myName_, iovIdx_ + 1, popIdx_);
            for (size_t i = 0; i <= iovIdx_; i++) {

                if (iovs_[i].length == 4){
                    tool::Logging(myName_, "\t[%zu]: len=%zu, buffer=%d\n", i, iovs_[i].length, *(int*)iovs_[i].buffer);
                }
                else if (iovs_[i].length == 8){
                    tool::Logging(myName_, "\t[%zu]: len=%zu, buffer=%p\n", i, iovs_[i].length, *(void**)iovs_[i].buffer);
                }
                else {
                    tool::Logging(myName_, "\t[%zu]: len=%zu, buffer=%p\n", i, iovs_[i].length, iovs_[i].buffer);
                }

                if (i == 0) {
                    for (size_t j = 0; j <= iovIdx_; j++) {
                        tool::Logging(myName_, "\t\t[%zu]: header=%zu, iov[0].buffer=%zu\n", j, headers_[j], ((size_t*)(iovs_[0].buffer))[j]);
                    }
                }
                
            }
        }
};

#endif
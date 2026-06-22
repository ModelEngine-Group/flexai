#ifndef REQUEST_BUFFER_H
#define REQUEST_BUFFER_H

#include "constVar.h"
#include "configure.h"

class RequestBuffer {
    private:
        const char* myName_ = "RequestBuffer";
        uint32_t popOffset_;  // point to the first empty space of the buffer
        uint32_t backOffset_; // [popOffset_, backOffset_) is the elements for the new request

    public:
        uint8_t* _dataBuffer;
        uint32_t _allocatedSize;

        RequestBuffer(uint32_t size, uint8_t* dataBuffer) : _allocatedSize(size), _dataBuffer(dataBuffer) {
            popOffset_ = 0;
            backOffset_ = size;
        }

        void Reset(uint32_t size, uint8_t* dataBuffer) {
            popOffset_ = 0;
            backOffset_ = sizeof(int) + size;
            _allocatedSize = sizeof(int) + size;
            _dataBuffer = dataBuffer; //!: after reset, the dataBuffer should be freed by the caller
        }

        void SetBackOffset(uint32_t offset) {
            backOffset_ = offset;
        }
    
        RequestBuffer(uint32_t size = REQUEST_BUFFER_SIZE) {
            popOffset_ = 0;
            backOffset_ = 0;
            _allocatedSize = sizeof(int) + size;
            _dataBuffer = (uint8_t*) malloc(_allocatedSize);
        }

        RequestBuffer(const RequestBuffer* otherBuf){
            popOffset_ = 0;
            backOffset_ = 0;
            _allocatedSize = otherBuf->_allocatedSize;
            _dataBuffer = (uint8_t*) malloc(_allocatedSize);
            memcpy(_dataBuffer, otherBuf->_dataBuffer, _allocatedSize);
        }

        ~RequestBuffer() {
            if (_dataBuffer != NULL){
                //todo: free(_dataBuffer);
            }
        }

        // return the allocated size of the buffer
        uint32_t GetSize() {
            return _allocatedSize;
        }

        void CheckFull(size_t size) {
            if ((backOffset_ + size) > _allocatedSize) {
                tool::Logging(myName_, "Already full: offset %zu, new element size %zu, total %zu\n", backOffset_, size, _allocatedSize);
                exit(EXIT_FAILURE);
            }
        }

        void CheckPopEnd(size_t size) {
            if ((popOffset_ + size) > backOffset_) {
                tool::Logging(myName_, "Already pop to the end: offset %zu, pop element size %zu, total %zu\n", popOffset_, size, backOffset_);
                exit(EXIT_FAILURE);
            }
        }

        size_t getRemainingSize() {
            return backOffset_ - popOffset_;
        }

        template <class T>
        void Push(T item) {
            CheckFull(sizeof(T));
            memcpy(_dataBuffer + backOffset_, (uint8_t*)&item, sizeof(T));
            backOffset_ += sizeof(T);
        }

        template <class T>
        void PushConst(const T item) {
            CheckFull(sizeof(T));
            memcpy(_dataBuffer + backOffset_, (uint8_t*)&item, sizeof(T));
            backOffset_ += sizeof(T);
        }

        template <class T>
        void Push64BitPointer(T item) {
            Push((uint64_t)item); //!: unified address is 64-bit
        }

        void PushHostAddr(const void* ptr){
            char* addrChar = new char[HOST_POINTER_SIZE];
#ifdef _WIN32
                sprintf_s(addrChar, 10, "%p", ptr);
#else
                sprintf(addrChar, "%p", ptr);
#endif
            PushCString(addrChar);
            delete[] addrChar;
        }

        void PushRequestType(int reqType){
            Push(reqType);
        }

        // ! notice that the num is the number of elements
        template <class T>
        void Push(T* item, size_t num = 1) {
            if (item == NULL) {
                Push((size_t)0);
                return;
            }
            size_t totalSize = sizeof(T) * num;
            Push(totalSize);
            memcpy(_dataBuffer + backOffset_, (uint8_t*)item, totalSize);
            backOffset_ += totalSize;
        }

        template <class T>
        void PushConst(const T* item, size_t num = 1) {
            if (item == NULL) {
                Push((size_t)0);
                return;
            }
            size_t totalSize = sizeof(T) * num;
            Push(totalSize); //! notice that the size is storage space size, not the number of elements
            memcpy(_dataBuffer + backOffset_, (uint8_t*)item, totalSize);
            backOffset_ += totalSize;
        }
        
        // push the len of char* , and then push all chars
        void PushCString(const char *s){
            size_t len = strlen(s) + 1; //! including the null-terminator
            PushConst(s, len);
        }

        template <class T>
        T Pop(){
            CheckPopEnd(sizeof(T));
            T res = *((T*)(_dataBuffer + popOffset_));
            popOffset_ += sizeof(T);
            return res;
        }

        // copy the array, and return the new begin address
        template <class T>
        T* Pop(size_t n) {
            if(Pop<size_t>() == 0) { // first, pop the size of the array
                return NULL;
            }
            CheckPopEnd(sizeof(T) * n);
            T* res = new T[n];
            memcpy((uint8_t*)res, _dataBuffer + popOffset_, sizeof(T) * n);
            popOffset_ += sizeof(T) * n;
            return res;
        }

        // copy the array to the given address
        template <class T>
        void Pop(T* res, size_t n) {
            if(Pop<size_t>() == 0) { // first, pop the size of the array
                return;
            }
            CheckPopEnd(sizeof(T) * n);
            memcpy((uint8_t*)res, _dataBuffer + popOffset_, sizeof(T) * n);
            popOffset_ += sizeof(T) * n;
        }

        template <class T>
        T* PopAll() {
            size_t size = Pop<size_t>(); // first, pop the size of the array
            if(size == 0) { 
                return NULL;
            }
            CheckPopEnd(size);
            T* res = (T*)malloc(size);
            memcpy((uint8_t*)res, _dataBuffer + popOffset_, size);
            popOffset_ += size;
            return res;
        }

        // pop the pointer with 64 bit address
        template <class T>
        T PopFromAddr() {
            return (T)Pop<uint64_t>();
        }

        // just assign the begin address, not copy the data
        template <class T>
        T* AssignAddr(size_t n = 1) {
            if(Pop<size_t>() == 0) { // first, pop the size of the array
                return NULL;
            }
            CheckPopEnd(sizeof(T) * n);
            T* res = (T*)(_dataBuffer + popOffset_);
            popOffset_ += sizeof(T) * n;
            return res;
        }
        
        template <class T>
        T* AssignAddrForAll() {
            size_t size = Pop<size_t>(); // first, pop the size of the array
            if(size == 0) { 
                return NULL;
            }
            CheckPopEnd(size);
            T* res = (T*)(_dataBuffer + popOffset_);
            popOffset_ += size;
            return res;
        }

        template <class T>
        T* AssignAddrForLast() {
            T* res = (T*)(_dataBuffer + popOffset_);
            popOffset_ = backOffset_;
            return res;
        }

        char* AssignCString(){
            return AssignAddrForAll<char>();
        }

        void Print() {
            tool::Logging(LOG_DEBUG, myName_, "RequestBuffer: popOffset_ = %u, backOffset_ = %u, _allocatedSize = %u\n", popOffset_, backOffset_, _allocatedSize);
            size_t i = 0;
            for (i = 0; i < backOffset_; i++) {
                printf("%02x ", _dataBuffer[i]);
            }
            printf("\n");
        }
};

#endif
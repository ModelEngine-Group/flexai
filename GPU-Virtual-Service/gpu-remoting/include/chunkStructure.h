#ifndef CHUNK_STRUCTURE_H
#define CHUNK_STRUCTURE_H

#include "constVar.h"
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <stdint.h>

typedef struct {
    char *name;
    size_t paramSize;
    size_t paramNum;
    uint16_t *paramOffsets;
    uint16_t *paramSizes;
    void *host_fun;
} KernelInfo_t; // used to store kernel parmeters from client fatCubin

struct KernelPtx_t {
    std::string name;
    std::string body;

    KernelPtx_t(const char* n, size_t name_len, const char* b, size_t body_len)
        : name(n, name_len), body(b, body_len) {}
}; // used to store kernel body from PTX codes

struct LdParamInfo_t {
    bool isUsed;
    size_t index;
    size_t offset;

    LdParamInfo_t(size_t idx, size_t off) : isUsed(false), index(idx), offset(off) {}
};

struct BatchInfo_t {
    uint8_t curType;
    size_t curBatchSize;
};

struct HostBuffer_t {
    uint8_t* hostPtr;
    size_t size;
};

struct TensorInfo_t {
    void* devPtr;
    size_t size;
};

struct Block_t{
    uint64_t start;
    uint64_t devPtr = 0;
    size_t size = 0;
    bool valid = false;
    bool essential = false;
};

struct Handle_t {
    uint64_t handlePtr = 0;
    enum API_REQUEST_CODE_SET type;
    bool valid = false;
    uint64_t stream = 0;
};

struct Sync_t {
    boost::mutex mutex;
    boost::condition_variable cv;
};

// struct GpuInform{
//     int GpuId;
//     char IpAddr [IP_STRING_LEN];
//     int Port;
// };

#endif //CHUNK_STRUCTURE_H
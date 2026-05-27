#ifndef BASICDEDUP_CONFIGURE_h
#define BASICDEDUP_CONFIGURE_h

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/atomic.hpp>
#include <boost/lockfree/queue.hpp>
#include <boost/interprocess/file_mapping.hpp>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/exceptions.hpp>
#include <boost/regex.hpp>
#include <boost/intrusive/list.hpp>
#include <sys/epoll.h>
#include "define.h"
#include "constVar.h"
#include "chunkStructure.h"
using namespace std;

class Configure {
private:
    string serverIp_;
    uint16_t serverPort_; 

    uint64_t clientID_;
    bool isClient_;
    size_t reqGPUnum_;
    size_t priority_;
    string proxyIp_;
    uint16_t proxyPort_;

    string dpcIp_;
    uint16_t dpcPort_;

    string monIp_;
    uint16_t monPort_;

    size_t DDPreqGPUnum_;

    string model_;
    size_t batchSize_;


    void ReadConf(std::string path);

public:
    Configure(std::string path, bool isClient = false);

    ~Configure();

    inline const string& GetServerIp() const noexcept {
        return serverIp_;
    }

    inline uint16_t GetServerPort() {
        return serverPort_;
    }

    inline uint64_t GetClientID() {
        return clientID_;
    }

    inline size_t GetReqGPUnum() {
        if (DDPreqGPUnum_ > 1) {
            return DDPreqGPUnum_;
        }
        return reqGPUnum_;
    }

    inline size_t GetPriority() {
        return priority_;
    }

    inline const string& GetProxyIp() const noexcept {
        return proxyIp_;
    }

    inline uint16_t GetProxyPort() {
        return proxyPort_;
    }

    inline const string& GetDpcIp() const noexcept {
        return dpcIp_;
    }

    inline uint16_t GetDpcPort() {
        return dpcPort_;
    }

    inline const string& GetMonIp() const noexcept {
        return monIp_;
    }

    inline uint16_t GetMonPort() {
        return monPort_;
    }

    inline size_t GetDDPreqGPUnum() {
        return DDPreqGPUnum_;
    }

    inline const string& GetModel() const noexcept {
        return model_;
    }

    inline size_t GetBatchSize() {
        return batchSize_;
    }


};

#endif
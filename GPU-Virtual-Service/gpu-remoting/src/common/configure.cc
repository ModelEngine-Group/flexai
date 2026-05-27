#include "../../include/configure.h"

Configure::~Configure() {
}

Configure::Configure(std::string path, bool isClient) {
    isClient_ = isClient;
    this->ReadConf(path);
}

void Configure::ReadConf(std::string path) {
    using namespace boost;
    using namespace boost::property_tree;
    
    ptree root;
    read_json<ptree>(path, root);

    serverIp_ = root.get<string>("ServerConfig.serverIp_");
    serverPort_ = root.get<uint16_t>("ServerConfig.serverPort_");

    dpcIp_ = root.get<string>("DispatcherConfig.dpcIp_");
    dpcPort_ = root.get<uint16_t>("DispatcherConfig.dpcPort_");

    monIp_ = root.get<string>("MonitorConfig.monitorIp_");
    monPort_ = root.get<uint16_t>("MonitorConfig.monitorPort_");

    if (isClient_) {
        const char* envClientID = std::getenv("FLEXGV_CLIENT_ID");
        const char* envPriority = std::getenv("FLEXGV_PRIORITY");
        const char* envReqGPUnum = std::getenv("FLEXGV_REQ_NUM");
        const char* envModel = std::getenv("FLEXGV_MODEL");
        const char* envBatchSize = std::getenv("FLEXGV_BATCH_SIZE");
        clientID_ = envClientID ? std::stoull(envClientID) : root.get<uint64_t>("ClientConfig.clientID_");
        reqGPUnum_ = root.get<size_t>("ClientConfig.requestGPUnum_");
        priority_ = envPriority ? std::stoull(envPriority) : root.get<size_t>("ClientConfig.priority_");
        proxyIp_ = root.get<string>("ClientConfig.proxyIp_");
        proxyPort_ = root.get<uint16_t>("ClientConfig.proxyPort_");
        DDPreqGPUnum_ = envReqGPUnum ? std::stoull(envReqGPUnum) : 1;
        model_ = envModel ? envModel : "resnet18";
        batchSize_ = envBatchSize ? std::stoull(envBatchSize) : 32;
    }  
    return ;
}

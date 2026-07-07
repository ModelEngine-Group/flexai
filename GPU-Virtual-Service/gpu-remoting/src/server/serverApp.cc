#include <chrono>
#include "../../include/configure.h"
#include "../../include/ucpConnection.h"
#include "../../include/serverEndpoint.h"
#include "../../include/conqueue/readerwriterqueue.h"
using namespace std;

Configure config_("config.json");
const char* myName = "ServerApp";
UCPConnection* connectionObj;
vector<boost::thread*> thList;
// moodycamel::BlockingReaderWriterQueue<UCPConnection_t> q(CONN_RESERVED_NUM);
volatile bool isClosed = false;

void CTRLC(int s) {
    tool::Logging(LOG_INFO, myName, "terminate the server with ctrl+c interrupt\n");
    __sync_add_and_fetch_8(&isClosed, 1);
}

void cleanup() {
    // for (auto it : thList) {
    //     it->join();
    //     delete it;
    // }
    // for (auto it : serverEpList) {
    //     delete it;
    // }
    // tool::Logging(LOG_INFO, myName, "clear all server endpoint object.\n");

    delete connectionObj;
    tool::Logging(LOG_INFO, myName, "close the network connection.\n");
}

int main(int argc, char* argv[]) {

    struct sigaction sigIntHandler;
    sigIntHandler.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sigIntHandler, 0); // avoid the server crash when writing to a closed connection
    sigIntHandler.sa_handler = CTRLC;
    sigaction(SIGINT, &sigIntHandler, 0); 

    // boost::thread* thTmp;
    // boost::thread_attributes attrs;
    // attrs.set_stack_size(THREAD_STACK_SIZE);

    int devCnt = 0;
    cudaError_t exit_code = cudaGetDeviceCount(&devCnt);
    if (exit_code != cudaSuccess) {
        tool::Logging(LOG_ERROR, myName, "cudaGetDeviceCount failed: %s\n", cudaGetErrorName(exit_code));
        exit(EXIT_FAILURE);
    }


    connectionObj = new UCPConnection(false);
    connectionObj->Listen(config_.GetServerIp(), config_.GetServerPort(), ServerEndpoint::CreateServerEp);
    connectionObj->WaitConnection(&isClosed);

    cleanup();

    // while(true) {    
    //     ucp_conn_request_h conn_request = NULL;
    //     uint64_t client_id = 0;
    //     char* clientIP = NULL;
    //     char* clientPort = NULL;
    //     connectionObj->WaitConnection(&conn_request, &client_id, &clientIP, &clientPort);

    //     // ucp_worker_h dataWorker;
    //     // connectionObj->InitWorker(&dataWorker);
    //     // both dataWorker and conn_request are pointers to the struct
    //     serverEndPointObj = new ServerEndpoint(connectionObj->CreateWorker(), conn_request, client_id, clientIP, clientPort);

    //     thTmp = new boost::thread(attrs, boost::bind(&ServerEndpoint::Run, serverEndPointObj));
    //     thList.emplace_back(thTmp);
    //     //serverEpList.push_back(serverEndPointObj);
    // }

    // boost::thread CreateServeEp([&]() {
    //     UCPConnection_t conn;
    //     while (!isClosed) {
    //         if (q.wait_dequeue_timed(conn, std::chrono::seconds(2))) { // wakeup for every 2 seconds
    //             serverEndPointObj = new ServerEndpoint(connectionObj->CreateWorker(), conn);
    //             thTmp = new boost::thread(attrs, boost::bind(&ServerEndpoint::NewRun, serverEndPointObj));
    //             thList.emplace_back(thTmp);
    //         }
    //     }
    // });

    // boost::thread WaitConn([&]() {
    //     while (!isClosed) {
    //         UCPConnection_t conn;
    //         connectionObj->WaitConnection(&conn, &isClosed);
    //         if (isClosed) {
    //             break;
    //         }
    //         q.enqueue(conn);
    //     }
    // });

    // WaitConn.join();
    // CreateServeEp.join();

    return 0;
}
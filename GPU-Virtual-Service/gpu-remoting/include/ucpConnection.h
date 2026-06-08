#ifndef UCP_CONNECTION_H
#define UCP_CONNECTION_H
#include "ucpUtil.h"
#include "configure.h"

class UCPConnection {
    private:
        string myName_ = "UCPConnection";

        // string serverIP_; // instead of c_str(), which points to a temporary buffer with same addr
        // uint16_t serverPort_;
        // string clientIP_;
        // uint16_t clientPort_;

        int connectionType_ = CLIENT_SERVER_SEND_RECV_DEFAULT;

        ucp_context_h  ucpContext_;     // shared by all workers
        ucp_worker_h   listenWorker_;   // worker for listener, only used for server
        ucp_listener_h listener_;       // listener for server

        size_t clientNum_ = 0;  

        int epollFd_ = -1;
        int workerFd_ = -1; 
        

    public:
        UCPConnection_t _listenCtx; // context for client connection, only used for server
        // struct sockaddr_storage _serverAddr;
        // struct sockaddr_storage _clientAddr;

        void InitWorker(ucp_worker_h *ucpWorker, uint64_t clientID = 0);
        ucp_worker_h CreateWorker(bool is_client=false, uint64_t clientID = 0);
        void SetConnWorker(ucp_worker_h worker);

        // UCPConnection(string address_str, uint16_t port, bool is_client = false);
        UCPConnection(bool is_client);

        ~UCPConnection();

        ucs_status_t Listen(const string& serverIP, uint16_t serverPort, ucp_listener_conn_callback_t callback);
        void WaitConnection(volatile bool* is_closed);
        void WaitConnection(UCPConnection_t* conn, volatile bool* is_closed);
};


#endif
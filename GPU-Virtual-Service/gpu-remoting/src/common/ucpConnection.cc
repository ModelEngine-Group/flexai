#include "../../include/ucpConnection.h"
#include "ucs/type/thread_mode.h"

/**
 * Create a ucp worker.
 */
void UCPConnection::InitWorker(ucp_worker_h *ucpWorker, uint64_t clientID) {
    ucp_worker_params_t worker_params;
    ucs_status_t status;

    memset(&worker_params, 0, sizeof(worker_params));
    worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_MULTI; // todo: change to multi-thread mode
    if (clientID != 0) {
        worker_params.field_mask |= UCP_WORKER_PARAM_FIELD_CLIENT_ID;
        worker_params.client_id   = clientID;
    }

    if((status = ucp_worker_create(ucpContext_, &worker_params, ucpWorker)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to create ucp worker (ucp_worker_create:%s)\n", ucs_status_string(status));
        ucp_cleanup(ucpContext_);
        exit(EXIT_FAILURE);
    } else {
        tool::Logging(LOG_DEBUG, myName_.c_str(), "ucp worker is created successfully.\n");
    }
}

ucp_worker_h UCPConnection::CreateWorker(bool is_client, uint64_t clientID) {
    ucp_worker_h ucpWorker;
    ucp_worker_params_t worker_params;
    ucs_status_t status;

    memset(&worker_params, 0, sizeof(worker_params));
    worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
    worker_params.thread_mode = UCS_THREAD_MODE_MULTI; // todo: change to multi-thread mode
    if (is_client) {
        worker_params.field_mask |= UCP_WORKER_PARAM_FIELD_CLIENT_ID;
        worker_params.client_id   = clientID;
    }

    if((status = ucp_worker_create(ucpContext_, &worker_params, &ucpWorker)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to create ucp worker (ucp_worker_create:%s)\n", ucs_status_string(status));
        ucp_cleanup(ucpContext_);
        exit(EXIT_FAILURE);
    } else {
        tool::Logging(LOG_DEBUG, myName_.c_str(), "ucp worker is created successfully.\n");
    }
    return ucpWorker;
}

/**
 * Initialize the UCP context and worker (for server listener).
 */
// UCPConnection::UCPConnection(string address_str, uint16_t port, bool is_client) {
//     /* IP & port */
//     serverIP_ = address_str;
//     serverPort_ = port;
    
//     /* UCP objects */
//     ucp_params_t ucp_params;
//     ucs_status_t status;
//     memset(&ucp_params, 0, sizeof(ucp_params));

//     /* UCP initialization */
//     ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES | UCP_PARAM_FIELD_NAME | UCP_PARAM_FIELD_MT_WORKERS_SHARED;
//     ucp_params.name       = "client_server";
//     ucp_params.features = UCP_FEATURE_AM; // default feature
//     if (!is_client) {
//         ucp_params.features |= UCP_FEATURE_WAKEUP;
//     }
//     ucp_params.mt_workers_shared = 1;

//     if((status = ucp_init(&ucp_params, NULL, &ucpContext_)) != UCS_OK) {
//         tool::Logging(LOG_ERROR, myName_.c_str(), "failed to init ucp context (ucp_init:%s)\n", ucs_status_string(status));
//         exit(EXIT_FAILURE);
//     }
//     else {
//         tool::Logging(LOG_DEBUG, myName_.c_str(), "ucp context is created successfully.\n");
//     }

//     _listenCtx.conn_request = NULL;
//     listener_ = NULL;

//     tool::SetSockAddr(serverIP_.c_str(), serverPort_, &_serverAddr, AF_INET);
//     if (is_client) {
//         // clientIP_ = clientIP;
//         // clientPort_ = clientPort;
//         // tool::SetSockAddr(clientIP_.c_str(), clientPort_, &_clientAddr, AF_INET);
//         listenWorker_ = nullptr;
//     }
//     else {
//         InitWorker(&listenWorker_); // server needs to create another worker for listening
//     }
// }

UCPConnection::UCPConnection(bool is_client) {
    /* UCP objects */
    ucp_params_t ucp_params;
    ucs_status_t status;
    memset(&ucp_params, 0, sizeof(ucp_params));

    /* UCP initialization */
    ucp_params.field_mask = UCP_PARAM_FIELD_FEATURES | UCP_PARAM_FIELD_NAME | UCP_PARAM_FIELD_MT_WORKERS_SHARED;
    ucp_params.name       = "client_server";
    ucp_params.features = UCP_FEATURE_AM; // default feature
    if (!is_client) {
        ucp_params.features |= UCP_FEATURE_WAKEUP;
    }
    ucp_params.mt_workers_shared = 1;

    if((status = ucp_init(&ucp_params, NULL, &ucpContext_)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to init ucp context (ucp_init:%s)\n", ucs_status_string(status));
        exit(EXIT_FAILURE);
    }
    else {
        tool::Logging(LOG_DEBUG, myName_.c_str(), "ucp context is created successfully.\n");
    }

    _listenCtx.conn_request = NULL;
    listener_ = NULL;

    if (is_client) {
        // clientIP_ = clientIP;
        // clientPort_ = clientPort;
        // tool::SetSockAddr(clientIP_.c_str(), clientPort_, &_clientAddr, AF_INET);
        listenWorker_ = nullptr;
    }
    else {
        InitWorker(&listenWorker_); // server needs to create another worker for listening
    }
}

void UCPConnection::SetConnWorker(ucp_worker_h worker){
    listenWorker_ = worker; // set the worker for the connection
}


ucs_status_t UCPConnection::Listen(const string& serverIP, uint16_t serverPort, ucp_listener_conn_callback_t callback){
    struct sockaddr_storage serverAddr;
    ucp_listener_params_t params;
    ucp_listener_attr_t attr;
    ucs_status_t status;
    char ip_str[IP_STRING_LEN], port_str[PORT_STRING_LEN];

    tool::SetSockAddr(serverIP.c_str(), serverPort, &serverAddr, AF_INET);

    params.field_mask         = UCP_LISTENER_PARAM_FIELD_SOCK_ADDR |
                                UCP_LISTENER_PARAM_FIELD_CONN_HANDLER;
    params.sockaddr.addr      = (const struct sockaddr*)&serverAddr;
    params.sockaddr.addrlen   = sizeof(serverAddr);
    // params.conn_handler.cb    = ServerConnHandleCallback;
    params.conn_handler.cb    = callback;
    params.conn_handler.arg   = this;

    /* Create a listener on the server side to listen on the given address */
    if ((status = ucp_listener_create(listenWorker_, &params, &listener_)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to listen (%s)\n", ucs_status_string(status));
        return status;
    }

    /* Query the created listener to get the IP & port it is listening on */
    attr.field_mask = UCP_LISTENER_ATTR_FIELD_SOCKADDR;
    if ((status = ucp_listener_query(listener_, &attr)) != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to query listener (%s)\n", ucs_status_string(status));
        ucp_listener_destroy(listener_);
        return status;
    }
    else {
        tool::GetIpStrFromSockaddr(&attr.sockaddr, ip_str, sizeof(ip_str));
        tool::GetPortStrFromSockaddr(&attr.sockaddr, port_str, sizeof(port_str));
        tool::Logging(LOG_INFO, myName_.c_str(), "server is listening on %s:%s\n", ip_str, port_str);
    }
    
    epoll_event ev;

    /* Create an epoll instance */
    epollFd_ = epoll_create1(0);
    if (epollFd_ == -1) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to create epoll instance\n");
        return UCS_ERR_IO_ERROR;
    }

    /* Get the file descriptor of the worker */
    status = ucp_worker_get_efd(listenWorker_, &workerFd_);
    if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to get UCX worker fd\n");
        return status;
    }

    /* Arm the worker to receive events */
    status = ucp_worker_arm(listenWorker_);
    if (status == UCS_ERR_BUSY) {
        tool::Logging(LOG_INFO, myName_.c_str(), "Events have already arrived\n");
    } else if (status != UCS_OK) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to arm UCX worker\n");
        return status;
    }

    /* Add the worker fd to the epoll */
    memset(&ev, 0, sizeof(ev));
    ev.events  = EPOLLIN;
    ev.data.fd = workerFd_;
    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, workerFd_, &ev) == -1) {
        tool::Logging(LOG_ERROR, myName_.c_str(), "failed to add UCX worker fd to epoll\n");
        return UCS_ERR_UNSUPPORTED;
    }

    return UCS_OK;
}

void UCPConnection::WaitConnection(volatile bool* is_closed){   
    epoll_event ev;
    int ret;

    while ((*is_closed) == false) { 
        // tool::Logging(LOG_INFO, myName_.c_str(), "waiting for a connection request from client...\n");

        /* push the worker to progress the listener */
        if (ucp_worker_progress(listenWorker_)) {
            continue;
        }

        /* wait for the worker to receive events */
        ret = epoll_wait(epollFd_, &ev, 1, -1);  // -1 means blocking
        if (ret == -1) {
            if (errno == EINTR) {
                continue;  // interrupted by signal
            } else {
                tool::Logging(LOG_ERROR, myName_.c_str(), "epoll_wait failed\n");
                break;
            }
        }

        /* handle the worker event */
        if (ev.data.fd == workerFd_) {
            ucp_worker_progress(listenWorker_);
        }

        /* re-arm the worker to receive the next event */
        ucs_status_t status = ucp_worker_arm(listenWorker_);
        if (status == UCS_ERR_BUSY) {
            // UCS_ERR_BUSY means there are unprocessed events, so continue to process
            continue;
        } else if (status != UCS_OK) {
            tool::Logging(LOG_ERROR, myName_.c_str(), "failed to arm UCX worker\n");
            break;
        }
    }
}


void UCPConnection::WaitConnection(UCPConnection_t* conn, volatile bool* is_closed){
    _listenCtx.conn_request = NULL; // reset the connection request
    tool::Logging(LOG_INFO, myName_.c_str(), "waiting for a connection request from client...\n");
    while ((*is_closed) == false && _listenCtx.conn_request == NULL) {
        ucp_worker_progress(listenWorker_);
    }
    if (*is_closed) {
        return;
    }
    clientNum_++;
    conn->conn_request = _listenCtx.conn_request;
    conn->client_id = _listenCtx.client_id;
    conn->client_ip = _listenCtx.client_ip;
    conn->client_port = _listenCtx.client_port;
    _listenCtx.conn_request = NULL;
}

UCPConnection::~UCPConnection() {
    if (listener_ != NULL) {
        ucp_listener_destroy(listener_);
    }
    if (listenWorker_ != NULL) {
        ucp_worker_destroy(listenWorker_);
    }
    ucp_cleanup(ucpContext_);
    tool::Logging(LOG_INFO, myName_.c_str(), "close the ucp connection.\n");
}


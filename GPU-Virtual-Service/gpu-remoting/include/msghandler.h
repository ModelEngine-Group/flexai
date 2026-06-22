#ifndef MSG_HANDLER_H
#define MSG_HANDLER_H

#include "configure.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cuda_device_runtime_api.h>
#include <cuda_runtime_api.h>
#include <netinet/in.h>
#include <nvml.h>
#include <driver_types.h>
#include <queue>
#include <set>
#include <string>
#include <strings.h>
#include <unistd.h>
#include <unordered_map>
#include <chrono>
#include <utility>
#include <cuda.h>
#include <vector>
#include <mutex>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "constVar.h"
#include "define.h"
#include <sys/un.h>
#include <unistd.h>

extern Configure config_;
using namespace std;

typedef struct Jobinfo {
    size_t used_mem; // memory
    uint sm_util; //SM utilization
    double served_time;//按秒算
    double remain_time;
    double perIt_time;
    int cur_It;// current iterations
    int num_It;// the number of iterations
    int device;
    uint64_t client_id;
    bool stop_listen_flag = false;
    int priority_;
    

    bool operator<(const Jobinfo& other) const {
        return remain_time < other.remain_time;
    }
} Jobinfo_;


class MsgHandler {
    private:
        Jobinfo_* job;
        const char *myName_ = "MsgHandler";
        int dispatch_sock = -1;
        int monitor_sock = -1;
    
    public:
        bool replay_flag = false;
        bool stop_flag = false;


        MsgHandler(uint64_t clientId) {
            tool::Logging(LOG_INFO, myName_, "MsgHandler() client %lu, MsgHandler is created.\n", clientId);
            job = new Jobinfo_();
            job->client_id = clientId;
            initialize_connection(config_.GetDpcIp(), config_.GetDpcPort(), dispatch_sock);

        }

        ~MsgHandler() {
            send_msg("TypeE:", 1);
            tool::Logging(LOG_INFO, myName_, "~MSGHandler() client %lu, MsgHandler is destroyed.\n", job->client_id);
            release_connection(dispatch_sock);
            delete job;
        }

        void initialize_connection(const std::string& ip, int port, int& sock) {
            if (sock == -1) {
                int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
                std::cout << "clientId: " << job->client_id << " tid:" << tmptid << " get in initialize_connection "  <<std::endl;
        
                struct sockaddr_in serv_addr;
        
                if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    std::cerr << "Socket creation error." << std::endl;
                    return;
                }
                memset(&serv_addr, 0, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(port);
        
                if (inet_pton(AF_INET, ip.c_str(), &serv_addr.sin_addr) <= 0) {
                    std::cerr << "Invalid address/ Address not supported." << std::endl;
                    return;
                }
        
                if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    std::cerr << "Connection Failed." << std::endl;
                    return;
                }
                std::string msg = "TypeC:" + to_string(job->client_id);
                send(sock, msg.c_str(), msg.size(), 0);
                
            }
        }

        void release_connection(int _sock) {
            // std::lock_guard<std::mutex> lock(conn_mutex);
            std::cout << "_sock:" << _sock << std::endl;
            int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
            if (_sock != -1) {
                std::cout << "clientId: " << job->client_id <<" tid:" << tmptid << " get in release_connection" << std::endl;
                close(_sock);
                tool::Logging(LOG_INFO, "Scheduler", "Disconnected from Monitor.\n");
            }
        }

        void send_msg(const std::string& message, int sock_flag){
            int tmp_sock = -1;
            if(sock_flag == 1){
                tmp_sock = dispatch_sock;
            }else if(sock_flag == 2){
                tmp_sock = monitor_sock;
            }   
            string msg = "";
            if (tmp_sock != -1) {
                if(message == "TypeE:"){
                    tool::Logging(LOG_INFO, myName_,"TypeE msg SendMeg::client %lu\n", job->client_id);
                    // if(replay_flag){
                    //     msg = message + to_string(job->client_id) + "," + to_string(1); 
                    // }else{
                    //     msg = message + to_string(job->client_id) + "," + to_string(0);
                    // }
                    msg = message + to_string(job->client_id) + "," + to_string(1); 
                    send(tmp_sock, msg.c_str(), msg.size(), 0);
                }
            }
        }

        
        bool receive_message(int sock, std::string& message) {
            char buffer[1024] = {0};
            int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
            if (bytes_received > 0) {
                message = std::string(buffer, bytes_received);
                if (message == "stop"){
                    // std::cout << "Received stop message." << std::endl;
                    tool::Logging(LOG_INFO,myName_,"client:%d, Received stop message.\n", job->client_id);
                    stop_flag = true;
                }
                return true;
            } else if (bytes_received == 0) {
                std::cout << "Connection closed by peer." << std::endl;
                return false;
            } else {
                // std::cerr << "Receive error." << std::endl;
                tool::Logging(LOG_ERROR,myName_,"client:%d, Receive error.\n", job->client_id);
                release_connection(sock);
                return false;
            }
        }

        void aysnc_receive_message() {
            std::string message;
            while (!job->stop_listen_flag) {
                if (receive_message(dispatch_sock, message)) {
                    tool::Logging(LOG_INFO,myName_,"client:%d, Received message: %s\n", job->client_id, message.c_str());
                    if (message == "stop") {
                        job->stop_listen_flag = true;
                    }
                }
            }
            tool::Logging(LOG_INFO,myName_,"client:%d, Stop listening.\n", job->client_id);
        }

};



#endif
#ifndef SCHEDULER_H
#define SCHEDULER_H

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
#define  GPU_MAX_NUM 8
#define  THRESH_ITER 11
// #define  TIME_QUANTUM 600
#define TIME_QUANTUM 360
extern Configure config_;
#define GPUNum 2

using namespace std;


typedef struct JobInfo {
    size_t used_mem; // memory
    uint sm_util; //SM utilization
    double served_time;//按秒算
    double remain_time;
    double perIt_time;
    int cur_It;// current iterations
    int num_It;// the number of iterations
    int device;
    uint64_t client_id;

    bool operator<(const JobInfo& other) const {
        return remain_time < other.remain_time;
    }
} JobInfo_;

class Scheduler {
    private:
        const char* myName_ = "Scheduler";
        static size_t free_mem[GPU_MAX_NUM];
        static size_t total_mem[GPU_MAX_NUM];
        static set<JobInfo_*> jobs;
        static queue<JobInfo_*> unknown_queue[GPU_MAX_NUM]; // 未知任务信息的队列 
        static vector<JobInfo_*> known_queue[GPU_MAX_NUM]; // 已知任务信息队列
        static unordered_map<JobInfo_*, int> known_map; // <job,token> token=1表示当前已知信息的任务在运行
        // static int flag;//判断是否有已知信息的任务在运行,0表示没有
        static int flag[GPUNum];//
        // static int queues;//0表示轮到unknown_queue，1表示轮到known_queue
        static int queues[GPUNum];//不同GPU下的队列情况,0表示轮到unknown_queue，1表示轮到known_queue
        // static chrono::steady_clock::time_point timer;//计时器，判断处理哪个队列
        static chrono::steady_clock::time_point timers[GPUNum];//计时器，判断处理哪个队列
        static set<uint64_t> client_ids;//防止重放入
        
        int dev_cnt;
        JobInfo_* job;
        pid_t pid;
        size_t* shm_util = 0;
        chrono::steady_clock::time_point start;
        int cur_pid;
        // int token = 0;//1表示当前已知信息的任务在运行

        int _sock = -1; // 用于连接monitor的静态socket
        
        //建立与monitor连接，并确保连接仅建立一次
        void initialize_connection() {

            if (_sock == -1) {
                int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
                std::cout << "clientId: " << job->client_id <<" tid:" << tmptid << " get in initalize_connection" << std::endl;

                struct sockaddr_in serv_addr;

                if ((_sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
                    tool::Logging(LOG_ERROR, "Scheduler", "Socket creation error.\n");
                    return;
                }
                memset(&serv_addr, 0, sizeof(serv_addr));
                serv_addr.sin_family = AF_INET;
                serv_addr.sin_port = htons(config_.GetMonPort());


                if (inet_pton(AF_INET, config_.GetMonIp().c_str(), &serv_addr.sin_addr) <= 0) {
                    tool::Logging(LOG_ERROR, "Scheduler", "Invalid address/ Address not supported.\n");
                    return;
                }

                if (connect(_sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
                    tool::Logging(LOG_ERROR, "Scheduler", "Connection Failed.\n");
                    return;
                }

                tool::Logging(LOG_INFO, "Scheduler", "Connected to Monitor.\n");

            }
        }

        void release_connection() {
            // std::lock_guard<std::mutex> lock(conn_mutex);
            std::cout << "_sock:" << _sock << std::endl;
            int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
            if (_sock != -1) {
                std::cout << "clientId: " << job->client_id <<" tid:" << tmptid << " get in release_connection" << std::endl;
                close(_sock);
                tool::Logging(LOG_INFO, "Scheduler", "Disconnected from Monitor.\n");
            }
        }

        void delayed_send(const std::string& message) {
            std::this_thread::sleep_for(std::chrono::seconds(3));  // 延迟发送，dummy? 发现初始化时间大概十几秒往上，甚至几分钟都有
            send(_sock, message.c_str(), message.size(), 0);
        }
        

        static std::mutex mtx_used[GPU_MAX_NUM];
        static std::mutex mtx_sche[GPU_MAX_NUM];
        static std::mutex mtx_exc[GPU_MAX_NUM];

    public:
        Scheduler(uint64_t clientId) {
            int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
            std::cout << "Scheduler::Scheduler() threadId = " << tmptid << "clientID: "<< clientId << std::endl;
            cudaGetDeviceCount(&dev_cnt);
            size_t free_byte, total_byte;
            
            for(int i = 0; i < dev_cnt; i++) {
                cudaSetDevice(i);
                cudaMemGetInfo(&free_byte, &total_byte);
                free_mem[i] = free_byte;
                total_mem[i] = total_byte;
            }
            std::set<JobInfo_*>::iterator iter;
            for(iter = jobs.begin(); iter != jobs.end(); ++iter) {
                if((*iter)->client_id == clientId) {
                    job = *iter;
                    tool::Logging(LOG_INFO,myName_,"get cid = %d\n",job->client_id);
                    break;
                }
            }
            if(iter == jobs.end() || jobs.empty()) {
                job = new JobInfo_();
                job->client_id = clientId;
            }
            // job->client_id = clientId;
            cur_pid = getpid();

#ifdef GV_Monitor
            //建立与Monitor的tcp连接
            initialize_connection();
#endif

        }

        ~Scheduler() {
            // tool::Logging(LOG_INFO,myName_,"client %lu delete\n",job->client_id);
            int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
            std::cout << "Scheduler::~Scheduler() threadId = " << tmptid << "clientID: "<< job->client_id << std::endl;

            delete job;
            // release_connection();
#ifdef GV_Monitor
            close(_sock);
#endif
        }
        
        void send_message(const std::string& message, int curDev_) {
            // std::lock_guard<std::mutex> lock(conn_mutex);
            int tmptid = static_cast<pid_t>(::syscall(SYS_gettid));
            std::cout << "sock:" << _sock << " tid: " << tmptid << std::endl;
            string msg = "";
            if (_sock != -1) {
                if(message == "client_runjob:"){
                    msg = message + to_string(curDev_);
                    // std::thread(&Scheduler::delayed_send, this, msg).detach();
                    send(_sock, msg.c_str(), msg.size(), 0);
                }else if(message == "client_stop:"){
                    msg = message + to_string(curDev_);
                    send(_sock, msg.c_str(), msg.size(), 0);
                }
                // send(_sock, message.c_str(), message.size(), 0);
                tool::Logging(LOG_INFO, "Scheduler", "Message sent: %s\n", msg.c_str());
            } else {
                tool::Logging(LOG_ERROR, "Scheduler", "Socket is not connected.\n");
            }
        }

        bool ready_to_del(){
            char buffer[256];
            size_t n = read(_sock, buffer, sizeof(buffer) - 1);
            if(n > 0) {
                buffer[n] = '\0';
                tool::Logging(LOG_INFO, "Scheduler", "Received message: %s\n", buffer);
                return true;
            }
            tool::Logging(LOG_ERROR, "Scheduler", "Failed to receive message.\n");
            return false;
        }
        

        int get_free_gpu() {
            int max_index = -1;
            int max_free = 0;
            
            for(int i = 0; i < dev_cnt; i++) {
                cudaSetDevice(i);
                size_t free_byte, total_byte;
                cudaMemGetInfo(&free_byte, &total_byte);
                if(max_free < free_byte) {
                    max_free = free_byte;
                    max_index = i;
                }
            }
            return max_index;
        }

        void enqueue(uint64_t client_id, int device) {
            //处理任务入队，可能是新任务，可能是已知信息任务。
            {
                std::lock_guard<std::mutex> lock(mtx_used[device]);
                if(timers[device] == std::chrono::steady_clock::time_point()){
                    tool::Logging(LOG_INFO, myName_, "device:%d , first time std::chrono::steady_clock::time_point\n", device);
                    timers[device] = chrono::steady_clock::now();
                }
                else {
                    chrono::steady_clock::time_point now_time = chrono::steady_clock::now();
                    int duration = (chrono::duration_cast<chrono::milliseconds>(now_time - timers[device]).count() * 1.0 / 1000);
                    // if(duration % 100 == 0){
                    //     tool::Logging(LOG_INFO, myName_, "duration = %d\n",duration);
                    // }
                    if(duration >= TIME_QUANTUM) {
                        queues[device] = (queues[device] == 0) ? 1 : 0;
                        tool::Logging(LOG_INFO, myName_, "device:%d, change to handle queue %d\n",device,queues[device]);
                        // SendNewInfoToDispatcher(config_);
                        timers[device] = chrono::steady_clock::now();
                    }
                }
            }
            
            if(jobs.count(job) == 0 && client_ids.count(job->client_id) == 0) { //新来的任务
                {
                    std::lock_guard<std::mutex> lock(mtx_used[device]);
                    if(queues[device] == 1 && known_queue[device].empty()) {
                        queues[device] = 0;
                        tool::Logging(LOG_INFO, myName_, "device:%d,there is no job in queue1, change to handle queue %d\n",device, queues[device]);
                    }
                }
                
                jobs.insert(job);
                client_ids.insert(job->client_id);
                unknown_queue[device].push(job);
                tool::Logging(LOG_INFO,myName_,"device:%d, client %lu insert unknown_queue, device:%d, flag = %d, cid = %lu, unknown_queue_size:%d\n",device, client_id, device ,flag[device], unknown_queue[device].front()->client_id,unknown_queue[device].size());
                int threadId = static_cast<pid_t>(::syscall(SYS_gettid));
                tool::Logging(LOG_INFO,myName_,"client %lu threadId = %d\n",client_id,threadId);

                while (unknown_queue[device].front() != job || flag[device] != 0 || queues[device] != 0) {
                    tool::Logging(LOG_INFO,myName_,"get int unknown_queue[%d].front() != job || flag != 0 || queues != 0,front() == %lu, flag == %d, queues = %d,\n",device, unknown_queue[device].front()->client_id, flag[device], queues[device]);
                    sleep(1);
                }
                //独占GPU
                if(unknown_queue[device].front() == job) {
                    // int device = get_free_gpu();
                    tool::Logging(LOG_INFO,myName_,"client %lu starts on gpu %d\n",job->client_id, device);
                    // cudaSetDevice(device);
                    job->device = device;
                    int shmid = shmget(1234, sizeof(int), IPC_CREAT | 0666);
                    if (shmid < 0) {
                        perror("shmget");
                        exit(EXIT_FAILURE);
                    }

                    shm_util = (size_t *)shmat(shmid, NULL, 0);
                    if ((intptr_t)shm_util == -1) {
                        perror("shmat");
                        exit(EXIT_FAILURE);
                    }
                    start = chrono::steady_clock::now();
                    pid = fork();
                    unknown_dequeue(pid);
                }
            }
            else {//已知任务信息的任务
                if (job->cur_It < THRESH_ITER + 1) {
                    // std::cout << "clientID:" << client_id << " get in if " << device <<std::endl;
                    return;
                }
                //cal_add_It过来，也就是任务已经完成了profiling，这时候需要将任务先从unknow_queue里出队，再插入到known_queue中
                else if (job->cur_It == (THRESH_ITER + 1) && unknown_queue[device].front() == job) {
                    // if(pid > 0) {
                    tool::Logging(LOG_INFO,myName_,"device:%d, finishing profiling client %lu\n", device, job->client_id);
                    unknown_dequeue(pid);
                    unknown_queue[device].pop();
                    // }
                    chrono::steady_clock::time_point now_time = chrono::steady_clock::now();
                    job->served_time = (chrono::duration_cast<chrono::milliseconds>(now_time - start).count() * 1.0 / 1000);
                    job->perIt_time = job->served_time / (THRESH_ITER - 1);
                    job->remain_time = (job->num_It - job->cur_It + 1) * job->perIt_time;
                    tool::Logging(LOG_INFO,myName_,"client %lu job Info: num_It=%d, cur_It=%d, sm_util=%d, mem=%zu, served_time = %lf,perIt_time = %lf,remain_time=%lf\n",
                    job->client_id, job->num_It, job->cur_It, job->sm_util, job->used_mem, job->served_time, job->perIt_time, job->remain_time);
                    known_queue[device].emplace_back(job);
                    tool::Logging(LOG_INFO, myName_, "device:%d, client %lu insert known_queue. unknown_queue size = %d\n",device, job->client_id, unknown_queue[device].size());
                    known_dequeue(device);
                    start = chrono::steady_clock::now();
                    tool::Logging(LOG_INFO, myName_, "client %lu restarts.\n", job->client_id);
                    // }
                }
                else {//选择剩余时间最小的任务
                    known_dequeue(device);
                }
            }
        }

        void unknown_dequeue(pid_t pid) {
            if(pid == 0) {//child process
                // int device;
                // cudaGetDevice(&device);
                nvmlReturn_t result;
                result = nvmlInit();
                if (result != NVML_SUCCESS) {
                    tool::Logging(LOG_ERROR,myName_,"client %lu ERROR: Failed to initialize NVML %d\n",job->client_id, result);
                    return;
                }
                int index = 0;
                cudaGetDevice(&index);
                nvmlDevice_t dev;
                result = nvmlDeviceGetHandleByIndex_v2(index, &dev);
                if(result != NVML_SUCCESS) {
                    tool::Logging(LOG_ERROR,myName_,"client %lu ERROR: nvmlDeviceGetHandleByIndex %d\n",job->client_id, result);
                    return;
                }
                int cnt = 0;
                while (true) {
                    nvmlUtilization_t util;
                    result = nvmlDeviceGetUtilizationRates(dev, &util);
                    if(result != NVML_SUCCESS) {
                        tool::Logging(LOG_ERROR,myName_,"client %lu ERROR: nvmlDeviceGetUtilizationRates\n",job->client_id);
                        return;
                    }
                    if(util.gpu > 0) {
                        job->sm_util += util.gpu;
                        // tool::Logging(LOG_INFO,myName_,"client %lu util = %d %d %d\n",job->client_id,util.gpu,util.gpu,job->sm_util);
                        ++cnt;
                        *shm_util = job->sm_util / cnt;
                        // *shm_util = max((uint)*shm_util, util.gpu);
                    }
 
                }
            }
            else if(pid > 0) {
                if(job->cur_It == (THRESH_ITER + 1)) {
                    if (kill(pid, SIGKILL) == 0) {
                        tool::Logging(LOG_INFO,myName_,"client %lu Sent SIGKILL to child process\n",job->client_id);
                    } else {
                        tool::Logging(LOG_ERROR,myName_,"client %lu ERROR: Failed to send SIGINT to child process\n",job->client_id);
                    }
                    job->sm_util = *shm_util;
                }
            }
            else {
                tool::Logging(LOG_ERROR,myName_,"client %lu fork error\n",job->client_id);
            }
        }

        void known_allocate(int device) {
            std::lock_guard<std::mutex> lock(mtx_sche[job->device]);
            // tool::Logging(LOG_INFO,myName_,"client %lu try to known_allocate\n",job->client_id);
            if (!flag[device]) {
                //按剩余时间从小到大排序
                sort(known_queue[device].begin(), known_queue[device].end());
                // tool::Logging(LOG_INFO,myName_,"client %lu finish sort\n",job->client_id);

                JobInfo_* shortest = known_queue[device][0];
                known_map[shortest] = 1;
                ++flag[device];
                size_t mem = shortest->used_mem;
                int util = shortest->sm_util;
                tool::Logging(LOG_INFO,myName_,"choose shortest job: cid = %lu, mem = %lu, util = %u\n",shortest->client_id, mem, util);

                //选择资源互补任务
                for(auto it = known_queue[device].begin(); it != known_queue[device].end(); ++it) {
                    tool::Logging(LOG_INFO,myName_,"jobInfo: cid = %lu, mem = %lu/%lu, util = %u\n",(*it)->client_id, (*it)->used_mem, total_mem[shortest->device],(*it)->sm_util,(*it)->remain_time);
                    if(it == known_queue[device].begin()){
                        continue;
                    }
                    JobInfo_* tmp = *it;
                    if(tmp->device != shortest->device)
                        continue;
                    if((tmp->used_mem + mem) <= total_mem[shortest->device] && (tmp->sm_util + util) <= 100 && known_map[tmp] == 0) {
                        known_map[tmp] = 1;
                        mem += tmp->used_mem;
                        util += tmp->sm_util;
                        ++flag[device];
                        tool::Logging(LOG_INFO,myName_,"choose resource-inter job: cid = %lu, mem = %lu, util = %u\n",tmp->client_id, mem, util);
                    }
                }
                // tool::Logging(LOG_INFO,myName_,"client %lu finish known_allocate\n",job->client_id);
            }
        }

        void known_dequeue(int device) {
            {
                std::lock_guard<std::mutex> lock(mtx_used[device]);
                if(queues[device] == 0 && unknown_queue[device].empty()) {
                    queues[device] = 1;
                    tool::Logging(LOG_INFO, myName_, "device[%d]there is no job in queue0, change to handle queue %d\n",device,queues[device]);
                }
            }
            
            while (!unknown_queue[device].empty() && queues[device] == 0) {//等待新来任务完成独占profiling
                if(known_map[job] == 1) {
                    --flag[device];
                    known_map[job] = 0;
                    //TODO
                    chrono::steady_clock::time_point now_time = chrono::steady_clock::now();
                    job->served_time += (chrono::duration_cast<chrono::milliseconds>(now_time - start).count() * 1.0 / 1000);
                    job->remain_time = (job->num_It - job->cur_It + 1) * job->perIt_time;
                    tool::Logging(LOG_INFO, myName_, "client id %lu block, there is a new job, id = %lu\n", job->client_id, unknown_queue[device].front()->client_id);
                }
                sleep(1);
            }
            if(known_map[job]){
                // std::cout << "get_in_known_job" << std::endl;
                return;
            }

        schedule_shortest:
            known_allocate(device);
            //flag != 0 有任务正在运行
            while(known_map[job] == 0) {
                sleep(1);
            }
            if(!flag[device])
                goto schedule_shortest;
            tool::Logging(LOG_INFO, myName_, "client %lu restarts.\n", job->client_id);
        }

        void cal_add_It(uint64_t client_id) {
            /*
            根据zss所说，每个iteration开始会调用两次cudaMemcpyAsyncH2DHandle，所以本函数的大概意思是，如果job->cur_It ==2，开始计时；
            THRESH_ITER+1的值为12，也就是结束了profiling，就会再次调用enqueue插入到known_queue中。
            */
            ++job->cur_It;
            if(job->cur_It == 2) {
                start = chrono::steady_clock::now();
            }
            if(job->cur_It == (THRESH_ITER + 1)) {
                int device;
                cudaGetDevice(&device);
                std::cout << "clientId:"<< client_id << " cal_add_It_device:" << device << std::endl;
                // tool::Logging(LOG_INFO, myName_, "cal_add_It_device_!!client %lu, Device: %d .\n", client_id, device);

                enqueue(client_id,device);
            }
        }

        void get_Iteration(int num) {
            job->num_It = int(num);
        }

        void cal_job_mem(size_t size) {
            job->used_mem += size;
        }

        void free_jobs(uint64_t client_id, int device) {
            if(unknown_queue[device].front() == job) {
                unknown_queue[device].pop();
                {
                    std::lock_guard<std::mutex> lock(mtx_used[device]);
                    if(queues[device] == 0 && unknown_queue[device].empty()) {
                        queues[device] = 1;
                    }
                }

            }
            else {
                for(auto it = known_map.begin(); it != known_map.end(); ++it) {
                    if((*it).second == 1) {
                        (*it).second = 0;
                        --flag[device];
                        chrono::steady_clock::time_point now_time = chrono::steady_clock::now();
                        job->served_time += (chrono::duration_cast<chrono::milliseconds>(now_time - start).count() * 1.0 / 1000);
                        job->remain_time = (job->num_It - job->cur_It + 1) * job->perIt_time;
                        tool::Logging(LOG_INFO, myName_, "client id %lu block, job %lu has finished\n", (*it).first->client_id,job->client_id);
                    }
                }
                auto it = known_map.find(job);
                if(it != known_map.end())
                    known_map.erase(it);
                auto it1 = find(known_queue[device].begin(), known_queue[device].end(), job);
                if(it1 != known_queue[device].end())
                    known_queue[device].erase(it1);
                std::lock_guard<std::mutex> lock(mtx_used[device]);
                if(queues[device] == 1 && known_queue[device].empty()) {
                    queues[device] = 0;
                }
            }

            auto it2 = jobs.find(job);
            if(it2 != jobs.end())
                jobs.erase(it2);
            // add_sub_unuse_men(device, total_m, 1);
            if (kill(pid, SIGKILL) == 0) {
                tool::Logging(LOG_INFO,myName_,"client %lu Sent SIGKILL to child process\n",job->client_id);
            } else {
                tool::Logging(LOG_ERROR,myName_,"client %lu ERROR: Failed to send SIGINT to child process\n",job->client_id);
            }
        }



};

inline size_t Scheduler::free_mem[GPU_MAX_NUM];
inline size_t Scheduler::total_mem[GPU_MAX_NUM];
inline set<JobInfo_*> Scheduler::jobs;
inline queue<JobInfo_*> Scheduler::unknown_queue[GPU_MAX_NUM]; // 未知任务信息的队列
inline vector<JobInfo_*> Scheduler::known_queue[GPU_MAX_NUM]; // 已知任务信息队列
inline int Scheduler::flag[GPUNum];//判断是否有已知信息的任务在运行,0表示没有
inline unordered_map<JobInfo_*, int> Scheduler::known_map;
inline int Scheduler::queues[GPUNum];//0表示轮到unknown_queue，1表示轮到known_queue
// inline chrono::steady_clock::time_point Scheduler::timer;
inline chrono::steady_clock::time_point Scheduler::timers[GPUNum];
inline set<uint64_t> Scheduler::client_ids;

inline std::mutex Scheduler::mtx_used[GPU_MAX_NUM];
inline std::mutex Scheduler::mtx_sche[GPU_MAX_NUM];

#endif
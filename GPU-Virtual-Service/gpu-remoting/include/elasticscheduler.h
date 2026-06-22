#include "configure.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cuda_device_runtime_api.h>
#include <cuda_runtime_api.h>
#include <iostream>
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
using namespace std;

#define GPU_MAX_NUMe 2
//高低优先级宏定义，可修改成其他值
#define HighPriority 0//高优先级
#define LowPriority 1//低优先级
#define ProfilingIter 20//Profiling的Iteration数

typedef struct eJob_Info{
    uint64_t client_id;
    int device;
    int priority;
    int lastIt;//记录上一次迭代数
    int curIt;//记录当前迭代数
    int totalIt;//记录总迭代数
    unordered_map<eJob_Info*, double> coloThrougput;//记录与当前高优先级作业的共置吞吐量
    chrono::steady_clock::time_point job_start_time;//记录当前作业开始时间
    int highColoIndex = 0;//高优先级作业用于记录当前profiling到第几个低优先级作业
    int finishProFlag = 0;//低优先级作业用于记录当前作业是否已经profiling
    //dummy
    int tmpFlag;

}eJob_Info_;


class eScheduler{
    private:
        const char* myName_ = "eScheduler";
        static set<eJob_Info_*> jobs;
        static chrono::steady_clock::time_point timers[GPU_MAX_NUMe];//计时器，判断处理哪个队列
        static unordered_map<eJob_Info*, int> JobIsRunning;//判断任务是否运行，JobIsRunning[job] = 1表示job正在运行
        static std::mutex mtx_used[GPU_MAX_NUMe];//一堆锁
        static std::mutex mtx_time[GPU_MAX_NUMe];
        static std::mutex mtx_low[GPU_MAX_NUMe];
        static std::mutex mtx_high[GPU_MAX_NUMe];
        static std::mutex mtx_del[GPU_MAX_NUMe];
        static vector<eJob_Info_*> HighPriorityQueue[GPU_MAX_NUMe];//高优先级队列，存放高优先级任务
        static vector<eJob_Info_*> LowPriorityQueue[GPU_MAX_NUMe];//低优先级队列，存放低优先级任务
        static eJob_Info_* nowHighjob[GPU_MAX_NUMe];//当前 <正在运行> 的 <高优先级任务>
        static int HighPriorityRunningFlag[GPU_MAX_NUMe];//当前有几个高优先级任务在运行
        static int LowPriorityRunningFlag[GPU_MAX_NUMe];//当前有几个低优先级任务在运行
        static int ColoRunningFlag[GPU_MAX_NUMe];//当前是否有任务在 <共置> 运行
        static int FinishProfiling[GPU_MAX_NUMe];//当前高优先级任务是否已经完成了所有的低优先级任务的profiling
        static int OneLastLowJob[GPU_MAX_NUMe];//当前是否只有一个低优先级任务，用于最后一个低优先级任务唤醒自己

        static vector<eJob_Info_*> LowPriorityWaitingQueue[GPU_MAX_NUMe];//低优先级队列，存放低优先级任务

        eJob_Info_* job;

    public:
        eScheduler(uint64_t clientId){
            tool::Logging(LOG_INFO,myName_,"eScheduler::eScheduler(): client_ID:%lu\n",clientId);
            job = new eJob_Info_();
            job->client_id = clientId;
        }

        ~eScheduler(){
            tool::Logging(LOG_INFO,myName_,"eScheduler::~eScheduler(): client_ID:%lu\n",job->client_id);
            delete job;
        }

        void enqueue(uint64_t client_id, int device, int priority){//每次接受一个劫持的API就调用一次
            if(jobs.count(job) == 0){//作业第一次到达。
                job->device = device;
                job->priority = priority;
                jobs.insert(job);//插入所有作业的队列
                if(job->priority == HighPriority){//高优先级任务
                    HighPriorityQueue[device].push_back(job);
                    JobIsRunning[job] = 0;
                    if(HighPriorityRunningFlag[device] == 0){//如果当前没有高优先级任务在运行,启动当前任务
                        HighPriorityRunningFlag[device]++;
                        JobIsRunning[job] = 1;
                        nowHighjob[device] = job;
                    }
                    // nowHighjob[device] = job;
                    tool::Logging(LOG_INFO,myName_,"device:%d, client %lu insert HighPriorityQueue, device:%d, HighPriorityQueue_size:%d\n",device, client_id, device ,HighPriorityQueue[device].size());
                }else if(job->priority == LowPriority){//低优先级任务
                    // LowPriorityQueue[device].push_back(job);
                    LowPriorityWaitingQueue[device].push_back(job);//低优先级任务先放在等待队列中
                    tool::Logging(LOG_INFO,myName_,"device:%d, client %lu insert LowPriorityWaitingQueue, device:%d, LowPriorityWaitingQueue_size:%d\n",device, client_id, device ,LowPriorityWaitingQueue[device].size());
                    while(LowPriorityQueue[device].size() >= 3 && LowPriorityQueue[device].front() != job){//如果当前有低优先级任务在运行，直接返回
                        sleep(2);
                    }
                    LowPriorityQueue[device].push_back(job);//低优先级任务放入低优先级队列中
                    LowPriorityWaitingQueue[device].erase(LowPriorityWaitingQueue[device].begin());//从等待队列中删除
                    tool::Logging(LOG_INFO,myName_,"device:%d, client %lu insert LowPriorityQueue, device:%d, LowPriorityQueue_size:%d, LowPriorityWaitingQueue_size:%d\n",device, client_id, device ,LowPriorityQueue[device].size(), LowPriorityWaitingQueue[device].size());
                    if(FinishProfiling[device] == 1){
                        FinishProfiling[device] = 0;
                    }
                    if(LowPriorityQueue[device].size() > 1){
                        OneLastLowJob[device] = 0;
                    }
                }
            }else{
                while(job->priority == HighPriority && JobIsRunning[job] == 0){
                    sleep(5);
                }
                if(job->curIt < 5){//前5个Iteration属于初始化阶段，不参与调度。
                    return;
                }
                if(job->curIt == 5 && job->priority == LowPriority){
                    job->lastIt = job->curIt;
                }
                while(job->priority == LowPriority && JobIsRunning[job] == 0 && HighPriorityRunningFlag[device] > 0){//高优先级任务突然到来的情况
                    if(OneLastLowJob[device]){//最后一个低优先级任务唤醒自己
                        JobIsRunning[job] = 1;
                        return;
                    }
                    sleep(1);
                }
                if(job->priority == LowPriority && FinishProfiling[device] == 0 && job->finishProFlag == 1 && JobIsRunning[job] == 1){
                    JobIsRunning[job] = 0;
                    LowPriorityRunningFlag[device]--;
                    tool::Logging(LOG_INFO,myName_,"new low priority job comming, stop 2 let new job profiling\n");
                }
                if(job->priority == LowPriority && HighPriorityRunningFlag[device] > 0 && LowPriorityRunningFlag[device] > 1 && JobIsRunning[job] == 1){//高优先级任务突然到来的情况，低优先级作业暂停自己
                    job->lastIt = job->curIt;
                    JobIsRunning[job] = 0;
                    LowPriorityRunningFlag[device]--;
                }
                {
                std::lock_guard<std::mutex> lock(mtx_used[device]);   
                    if(job->priority == HighPriority && FinishProfiling[device] == 1 && LowPriorityRunningFlag[device] == 0){
                        ToChooseBestColoJob(device);
                    }
                    if(job->priority == HighPriority && job->highColoIndex >= LowPriorityQueue[device].size() && LowPriorityRunningFlag[device] == 0 && FinishProfiling[device] == 0){
                        tool::Logging(LOG_INFO,myName_,"HighPriority Job:%lu has new Job to profiling\n", nowHighjob[device]->client_id);
                        // nowHighjob[device] = HighPriorityQueue[device][0];
                        job->highColoIndex = LowPriorityQueue[device].size() - 1;
                        auto lowJob = LowPriorityQueue[device][job->highColoIndex];
                        tool::Logging(LOG_INFO,myName_,"choosing low priority job 2 profiling : %lu\n",lowJob->client_id);
                        JobIsRunning[lowJob] = 1;
                        LowPriorityRunningFlag[device]++;
                        lowJob->lastIt = lowJob->curIt;//记录当前迭代数
                    }
                    if(job->priority == HighPriority && job->highColoIndex < LowPriorityQueue[device].size() && LowPriorityRunningFlag[device] == 0){
                            nowHighjob[device] = HighPriorityQueue[device][0];
                            auto lowJob = LowPriorityQueue[device][job->highColoIndex];
                            tool::Logging(LOG_INFO,myName_,"choosing low priority job 2 profiling : %lu\n",lowJob->client_id);
                            JobIsRunning[lowJob] = 1;
                            LowPriorityRunningFlag[device]++;
                            lowJob->lastIt = lowJob->curIt;//记录当前迭代数
                    }
                }
                if(job->priority == LowPriority && job->curIt - job->lastIt == 0 && HighPriorityRunningFlag[device] != 0){ 
                    if(job->job_start_time == chrono::steady_clock::time_point()){
                        tool::Logging(LOG_INFO,myName_,"client id %lu start profiling\n",job->client_id);
                        job->job_start_time = chrono::steady_clock::now();
                        nowHighjob[device]->lastIt = nowHighjob[device]->curIt;   
                    }
                }else if(job->priority == LowPriority && job->curIt - job->lastIt == ProfilingIter && job->finishProFlag == 0 && HighPriorityRunningFlag[device] != 0){
                    JobIsRunning[job] = 0;
                    chrono::steady_clock::time_point now_time = chrono::steady_clock::now();
                    // int duration = (chrono::duration_cast<chrono::milliseconds>(now_time - job->job_start_time).count() * 1.0 / 1000);
                    double duration = (chrono::duration_cast<chrono::milliseconds>(now_time - job->job_start_time).count() * 1.0 / 1000);
                    tool::Logging(LOG_INFO,myName_,"--------------------------------------------------------\n");
                    tool::Logging(LOG_INFO,myName_,"duration:%f\n",duration);
                    tool::Logging(LOG_INFO,myName_,"nowHighjob->curIt:%d\n",nowHighjob[device]->curIt - nowHighjob[device]->lastIt);
                    job->coloThrougput[nowHighjob[device]] = 1.0 * 20 / duration;
                    tool::Logging(LOG_INFO,myName_,"LowPriority Job:client id %lu coloThrougput:%f\n",job->client_id,job->coloThrougput[nowHighjob[device]]);
                    nowHighjob[device]->coloThrougput[job] = 1.0 * (nowHighjob[device]->curIt - nowHighjob[device]->lastIt) / duration;
                    tool::Logging(LOG_INFO,myName_,"HighPriority Job: client id %lu coloThrougput:%f\n",nowHighjob[device]->client_id,nowHighjob[device]->coloThrougput[job]);
                    nowHighjob[device]->highColoIndex++;
                    LowPriorityRunningFlag[device]--;
                    job->job_start_time = chrono::steady_clock::time_point();
                    job->finishProFlag = 1;
                    if(nowHighjob[device]->highColoIndex == LowPriorityQueue[device].size()){
                        tool::Logging(LOG_INFO,myName_,"HighPriority Job:%lu Finish all Profiling\n", nowHighjob[device]->client_id);
                        FinishProfiling[device] = 1;
                    }
                    tool::Logging(LOG_INFO,myName_,"LowPriorityRunningFlag:%d\n",LowPriorityRunningFlag[device]);
                    // std::cout << "finishflag:" << FinishProfiling[device] << "low runing num :"<< LowPriorityRunningFlag[device] << std::endl;
                    tool::Logging(LOG_INFO,myName_,"--------------------------------------------------------\n");
                }
                {
                    std::lock_guard<std::mutex> lock(mtx_low[device]);
                    if(job->priority == LowPriority && JobIsRunning[job] == 1 && HighPriorityRunningFlag[device] == 0 && LowPriorityRunningFlag[device] == 1 && !OneLastLowJob[device]){
                        for(auto it = LowPriorityQueue[device].begin(); it != LowPriorityQueue[device].end(); ++it){//启动所有低优先级作业
                            if((*it)->client_id == job->client_id){
                                continue;
                            }
                            auto tmplowjob = *it;
                            JobIsRunning[tmplowjob] = 1;
                            LowPriorityRunningFlag[device]++;
                            tool::Logging(LOG_INFO,myName_,"starting low job : %lu , LowPriorityRunningFlag:%d\n",tmplowjob->client_id,LowPriorityRunningFlag[device]);
                        }
                        tool::Logging(LOG_INFO,myName_,"all low jobs are running\n");
                    }
                }
                
            }
        }

        void ToChooseBestColoJob(int device){    
            std::lock_guard<std::mutex> lock(mtx_del[device]);
            if(ColoRunningFlag[device] != 0 && LowPriorityRunningFlag[device] != 0){
                return;
            }

            if(nowHighjob[device]->coloThrougput.size() == 0){
                return;
            }

            auto max_element = std::max_element(
                nowHighjob[device]->coloThrougput.begin(), nowHighjob[device]->coloThrougput.end(),
                [](const std::pair<eJob_Info*, double>& a, const std::pair<eJob_Info*, double>& b) {
                    return a.second < b.second;
                }
            );
            auto nowLowJob = max_element->first;
            tool::Logging(LOG_INFO,myName_,"choosing best colo job: %lu\n",nowLowJob->client_id);
            JobIsRunning[nowLowJob] = 1;
            LowPriorityRunningFlag[device]++;
            ColoRunningFlag[device]++;
        }

        void cal_add_It(uint64_t client_id){
            ++job->curIt;
        }

        void get_Iteration(int num){
            job->totalIt = int(num);
        }

        void free_jobs(uint64_t client_id, int device){
            if(job->priority == HighPriority){
                std::lock_guard<std::mutex> lock(mtx_high[device]);
                for(auto it = jobs.begin(); it != jobs.end(); ++it){
                    auto tmp = (*it)->coloThrougput.find(job);
                    if(tmp != (*it)->coloThrougput.end()){
                        (*it)->coloThrougput.erase(tmp);
                    }
                }
                HighPriorityRunningFlag[device]--;
                for(auto it = HighPriorityQueue[device].begin(); it != HighPriorityQueue[device].end(); ++it){
                    if((*it)->client_id == client_id && (*it)->device == device){
                        HighPriorityQueue[device].erase(it);
                        tool::Logging(LOG_INFO,myName_,"free clientId:%lu free job\n",client_id);
                        break;
                    }
                }
                if(HighPriorityQueue[device].size() == 0){
                    nowHighjob[device] = NULL;
                }else{
                    nowHighjob[device] = HighPriorityQueue[device][0];
                    JobIsRunning[nowHighjob[device]] = 1;
                    HighPriorityRunningFlag[device]++;
                    for(auto it = LowPriorityQueue[device].begin(); it != LowPriorityQueue[device].end(); ++it){//启动下一个高优先级作业，对所有低优先作业重新profiling
                        tool::Logging(LOG_INFO,myName_,"Stopping Low job : %lu\n",(*it)->client_id);
                        (*it)->finishProFlag = 0;
                        if(JobIsRunning[(*it)] == 1){
                            JobIsRunning[(*it)] = 0;
                            LowPriorityRunningFlag[device]--;
                        }
                        (*it)->lastIt = (*it)->curIt;
                    }
                    FinishProfiling[device] = 0;
                }
                for(auto it = LowPriorityQueue[device].begin(); it != LowPriorityQueue[device].end(); ++it){
                    (*it)->coloThrougput.erase(job);
                }
            }else if(job->priority == LowPriority){
                for(auto it = LowPriorityQueue[device].begin(); it != LowPriorityQueue[device].end(); ++it){
                    if((*it)->client_id == client_id && (*it)->device == device){
                        LowPriorityQueue[device].erase(it);
                        tool::Logging(LOG_INFO,myName_,"free clientId:%lu free job\n",client_id);
                        break;
                    }
                }
                {
                std::lock_guard<std::mutex> lock(mtx_used[device]);    
                    for(auto it = jobs.begin(); it != jobs.end(); ++it){
                        auto tmp = (*it)->coloThrougput.find(job);
                        if(tmp != (*it)->coloThrougput.end()){
                            (*it)->coloThrougput.erase(tmp);
                        }
                    }
                    if(LowPriorityRunningFlag[device] > 0){
                        LowPriorityRunningFlag[device]--;
                    }
                    ColoRunningFlag[device]--;
                    // if(FinishProfiling[device] == 0 && HighPriorityRunningFlag[device] == 1){//说明该低优先级任务在Profiling的时候就结束了执行
                    //     nowHighjob[device]->highColoIndex--;
                    // }
                    if(LowPriorityQueue[device].size() == 1){
                        OneLastLowJob[device] = 1;
                    }
                }
            }

            auto it = JobIsRunning.find(job);
            if(it != JobIsRunning.end()){
                JobIsRunning.erase(it);
            }

            for(auto it = jobs.begin(); it != jobs.end(); ++it){
                auto tmp = (*it)->coloThrougput.find(job);
                if(tmp != (*it)->coloThrougput.end()){
                    (*it)->coloThrougput.erase(tmp);
                }
            }
            for(auto it = jobs.begin(); it != jobs.end(); ++it){
                if((*it)->client_id == client_id && (*it)->device == device){
                    jobs.erase(it);
                    tool::Logging(LOG_INFO,myName_,"free clientId:%lu free job\n",client_id);
                    break;
                }
            }
        }

};




inline set<eJob_Info_*> eScheduler::jobs;
inline chrono::steady_clock::time_point eScheduler::timers[GPU_MAX_NUMe];
inline unordered_map<eJob_Info*, int> eScheduler::JobIsRunning;
inline std::mutex eScheduler::mtx_used[GPU_MAX_NUMe];
inline std::mutex eScheduler::mtx_time[GPU_MAX_NUMe];
inline std::mutex eScheduler::mtx_low[GPU_MAX_NUMe];
inline std::mutex eScheduler::mtx_high[GPU_MAX_NUMe];
inline std::mutex eScheduler::mtx_del[GPU_MAX_NUMe];
inline vector<eJob_Info_*> eScheduler::HighPriorityQueue[GPU_MAX_NUMe];
inline vector<eJob_Info_*> eScheduler::LowPriorityQueue[GPU_MAX_NUMe];
inline eJob_Info_* eScheduler::nowHighjob[GPU_MAX_NUMe];
inline int eScheduler::HighPriorityRunningFlag[GPU_MAX_NUMe];
inline int eScheduler::LowPriorityRunningFlag[GPU_MAX_NUMe];
inline int eScheduler::ColoRunningFlag[GPU_MAX_NUMe];
inline int eScheduler::FinishProfiling[GPU_MAX_NUMe];
inline int eScheduler::OneLastLowJob[GPU_MAX_NUMe];
inline vector<eJob_Info_*> eScheduler::LowPriorityWaitingQueue[GPU_MAX_NUMe];
#ifndef ASYNC_REQUEST_H
#define ASYNC_REQUEST_H

#include "configure.h"
#include <boost/function.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/thread_functors.hpp>

class AsyncRequest{
    private:
        const char* myName_ = "AsyncRequest";
        boost::mutex _mutex;                 
        boost::condition_variable _cv;       // wake up waiting thread
        bool _stop;                          // stop the worker thread
        bool _newReq;                        // whether there is a new request
        boost::thread* _worker = nullptr;     // worker thread

    public: 
        AsyncRequest() : _stop(false), _newReq(false) {
        }

        void Start(boost::function<void()> func){
            boost::thread_attributes attrs;
            attrs.set_stack_size(THREAD_STACK_SIZE);
            _worker = new boost::thread(attrs, func);
        }

        inline bool CheckStart() {
            return _worker != nullptr;
        }

        void Wait() {
            boost::unique_lock<boost::mutex> lock(_mutex);
            _cv.wait(lock, [this] {
                return !_newReq; // wait until there is a new request
            });
        }

        void Notify() {
            boost::unique_lock<boost::mutex> lock(_mutex);
            _newReq = !_newReq; // set new request flag 
            _cv.notify_one(); // notify the worker thread
        }

        bool Check() {
            boost::unique_lock<boost::mutex> lock(_mutex);
            _cv.wait(lock, [this] {
                return _newReq || _stop;
            });
            if (_stop && !_newReq) { // receive stop signal and no new request
                return false;
            }
            return true;
        }

        bool CheckStop(boost::chrono::seconds timeout) {
            boost::unique_lock<boost::mutex> lock(_mutex);
            bool res = _cv.wait_for(lock, timeout, [this] {
                return _stop;
            });
            return res;
        }

        void Stop() {
            boost::unique_lock<boost::mutex> lock(_mutex);
            _stop = true;
            _cv.notify_one();
        }

        void Lock() {
            _mutex.lock();
        }

        void Unlock() {
            _mutex.unlock();
        }

        ~AsyncRequest(){
            Stop();
            tool::Logging(LOG_DEBUG, myName_, "ready to destroy\n");
            if (_worker != nullptr) {
                if (_worker->joinable()) {
                    _worker->join();
                }
                tool::Logging(LOG_DEBUG, myName_, "worker thread is joined\n");
                delete _worker;
            }
            tool::Logging(LOG_DEBUG, myName_, "finish destroying\n");
        }
};

#endif // ASYNC_REQUEST_H
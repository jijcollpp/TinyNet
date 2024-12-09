#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <exception>
#include <pthread.h>
#include <list>
#include "locker.h"
#include "cond.h"

//using namespace std;

#define MAX_FD 65536

template<typename T> 
class threadPool
{
public:
    threadPool(int threadNum = 5);
    ~threadPool();

    bool append(T* conn_);

private:
    static void* work(void* arg);
    void run();

private:
    pthread_t* threadArr;
    std::list<T*> workqueue;
    bool stop;
    mutexLock mutex_;
    cond cond_;
};

template<typename T> 
threadPool<T>::threadPool(int threadNum):stop(false), mutex_(), cond_(mutex_)
{
    if(threadNum <= 0){
        throw std::exception();
    }

    threadArr = new pthread_t[threadNum];
    if(!threadArr){
        throw std::exception();
    }

    for (int  i = 0; i < threadNum; i++)
    {
        if(pthread_create(threadArr+i, NULL, work, this) != 0){
            delete [] threadArr;
            throw std::exception();
        }
        printf("create the %dth thread\n", i);

        if(pthread_detach(threadArr[i])){
            delete [] threadArr;
            throw std::exception();
        }
    }
    
}

template<typename T> 
threadPool<T>::~threadPool()
{
    delete [] threadArr;
    stop = true;
}

template<typename T> 
void* threadPool<T>::work(void* arg)
{
    threadPool* pool = (threadPool*)arg;
    pool->run();
    return pool;
}

template<typename T> 
void threadPool<T>::run()
{
    while(!stop)
    {
        {
            locker locker_(mutex_);
            cond_.wait();
        }

        T* n_work = nullptr;
        {
            locker locker_(mutex_);
            if(workqueue.empty()){
                continue;
            }

            n_work = workqueue.front();

            workqueue.pop_front();
        }

        n_work->process();
    }
}

template<typename T>
bool threadPool<T>::append(T* conn_)
{
    locker locker_(mutex_);
    if(workqueue.size() > MAX_FD){
        printf("server is busy!\n");
        return false;
    }
    workqueue.push_back(conn_);

    cond_.notify();
    return true;
}

#endif
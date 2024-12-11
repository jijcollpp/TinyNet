#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <exception>
#include <pthread.h>
#include <list>
#include "locker.h"
#include "cond.h"

using namespace std;

template<typename T> 
class threadPool
{
public:
    threadPool(int threadNum = 5);
    ~threadPool();

private:
    static void* work(void* arg);
    void run();

private:
    pthread_t* threadArr;
    list<T> workqueue;
    bool stop;
    mutex mutex_;
    cond cond_;
};

template<typename T> 
threadPool<T>::threadPool(int threadNum):stop(false), mutex_(), cond_(mutex_)
{
    if(threadNum <= 0){
        throw exception();
    }

    threadArr = new pthread_t[threadNum];
    if(!threadArr){
        throw exception();
    }

    for (int  i = 0; i < threadNum; i++)
    {
        if(pthread_create(threadArr+i, NULL, work, this) != 0){
            delete [] threadArr;
            throw exception();
        }
        printf("create the %dth thread\n", i);

        if(pthread_detach(threadArr[i])){
            delete [] threadArr;
            throw exception();
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

        T n_work;
        {
            locker locker_(mutex_);
            if(workqueue.empty()){
                continue;
            }

            n_work = workqueue.front();

            workqueue.pop_front();
        }

        //TODO: n_work.work();
    }
    
}

#endif
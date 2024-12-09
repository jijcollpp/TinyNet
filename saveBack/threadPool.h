#ifndef _THREADPOOL_H_
#define _THREADPOOL_H_
#include <exception>
#include <pthread.h>

using namespace std;

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
    bool stop;
};

threadPool::threadPool(int threadNum):stop(false)
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

threadPool::~threadPool()
{
    delete [] threadArr;
    stop = true;
}

void* threadPool::work(void* arg)
{
    threadPool* pool = (threadPool*)arg;
    pool->run();
    return pool;
}

void threadPool::run()
{
    while(!stop)
    {
        /* code */
    }
    
}

#endif
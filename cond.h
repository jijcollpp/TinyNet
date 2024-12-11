#ifndef _COND_H_
#define _COND_H_
#include <exception>
#include <pthread.h>
#include "locker.h"

class cond
{
public:
    cond(mutex& mutex):mutex_(mutex){
        if(pthread_cond_init(&cond_, NULL) != 0){
            throw std::exception();
        }
    }

    ~cond(){
        pthread_cond_destroy(&cond_);
    }

    void notify(){
        pthread_cond_signal(&cond_);
    }

    void wait(){
        //mutex_.lock();
        int ret = pthread_cond_wait(&cond_, mutex_.getmutex());
        //mutex_.unlock();
    }

private:
    mutex& mutex_;
    pthread_cond_t cond_;
};

#endif
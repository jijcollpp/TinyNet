#ifndef _LOCKER_H_
#define _LOCKER_H_
#include <exception>
#include <pthread.h>

class locker
{
public:
    explicit locker(mutex& mutex):mutex_(mutex)
    {
        mutex_.lock();
    };

    ~locker()
    {
        mutex_.unlock();
    };

private:
    mutex& mutex_;
};

class mutex
{
public:
    mutex(){
        if(pthread_mutex_init(&mutex_, NULL) != 0){
            throw std::exception();
        }
    }

    ~mutex(){
        pthread_mutex_destroy(&mutex_);
    }

    void lock(){
        pthread_mutex_lock(&mutex_);
    }

    void unlock(){
        pthread_mutex_unlock(&mutex_);
    }

    pthread_mutex_t* getmutex(){
        return &mutex_;
    }

private:
    pthread_mutex_t mutex_;
};

#endif
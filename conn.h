#ifndef _CONN_H_
#define _CONN_H_
#include <unistd.h>

#define BUFFER_SIZE 1024

class conn
{
public:
    conn(/* args */);
    ~conn();

    void init(int clientfd_);

    void process();

private:
    char recvBuffer_[BUFFER_SIZE];
    int fd_;
};

conn::conn():fd_(-1)
{
}

conn::~conn()
{
    close(fd_);
    fd_ = -1;
}

void conn::init(int clientfd_)
{
    fd_ = clientfd_;
}

void conn::process()
{
    memset(recvBuffer_, '\0', BUFFER_SIZE);

    int read_bytes = recv(fd_, recvBuffer_, sizeof(recvBuffer_), 0);
    if(read_bytes > 0){
        printf("%dusers: %s\n", fd_, recvBuffer_);
    }
    else if(read_bytes <= 0)
    {
        close(fd_);
        fd_ = -1;
    }
}

#endif
#ifndef _CONN_H_
#define _CONN_H_
#include <unistd.h>
#include <errno.h>

#define RECV_BUFFER_SIZE 1024

class conn
{
public:
    conn(/* args */);
    ~conn();

    void init(int clientfd_);

    bool read();
    void process();
    void close_conn();

public:
    //static int ;

private:
    int fd_;
    char recvBuffer_[RECV_BUFFER_SIZE];
    int read_idx;
};

conn::conn():fd_(-1)
{
}

conn::~conn()
{
    fd_ = -1;
}

// void conn::close_conn(){
//     epoll_ctl(fd_, EPOLL_CTL_DEL, fd_);
//     fd_ = -1;
// }

void conn::init(int clientfd_)
{
    fd_ = clientfd_;

    read_idx = 0;
    memset(recvBuffer_, '\0', RECV_BUFFER_SIZE);
}

bool conn::read(){
    if(read_idx >= RECV_BUFFER_SIZE){
        return false;
    }

    int read_bytes = 0;
    while(true){
        read_bytes = recv(fd_, recvBuffer_+read_idx, RECV_BUFFER_SIZE-read_idx, 0);
        if(read_bytes == -1){
            if(errno == EAGAIN || errno == EWOULDBLOCK){
                break;
            }
        }
        else if(read_bytes == 0)
        {
            return false;
        }

        read_idx += read_bytes;
        printf("%dusers: %s\n", fd_, recvBuffer_);
    }
    return true;
}

void conn::process()
{
    
}

#endif
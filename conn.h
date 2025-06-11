#ifndef _CONN_H_
#define _CONN_H_
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include <sys/epoll.h>
#include <sys/socket.h>

#define READ_BUFFER_SIZE 1024

class conn
{
public:
    conn(/* args */){};
    ~conn(){};

    void init(int clientfd_);

    bool read();
    void process();
    void close_conn();

public:
    static int m_epollfd;

private:
    int fd_;
    char readBuffer_[READ_BUFFER_SIZE];
    int read_idx;
};

#endif
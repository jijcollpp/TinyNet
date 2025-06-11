#ifndef _CONN_H_
#define _CONN_H_
#include <unistd.h>
#include <errno.h>
#include <string.h> //bzero/memset
#include <stdio.h> //printf

#include <sys/epoll.h>
#include <sys/socket.h> //socket/bind/listen/accept/recv
#include <fcntl.h> //setnonblocking

#define READ_BUFFER_SIZE 1024

class conn
{
public:
    conn(/* args */){};
    ~conn(){};

    void init(int clientfd_);

    bool read();
    bool write();
    void process();
    void close_conn();

public:
    static int m_epollfd;
    static int m_user_count;

private:
    int fd_;
    char readBuffer_[READ_BUFFER_SIZE];
    int read_idx;
};

#endif
#include "conn.h"

int conn::m_epollfd = -1;

void conn::close_conn(){
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd_, 0);
    fd_ = -1;
}

void conn::init(int clientfd_)
{
    fd_ = clientfd_;

    read_idx = 0;
    memset(readBuffer_, '\0', READ_BUFFER_SIZE);
}

bool conn::read(){
    if(read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    int read_bytes = 0;
    while(true){
        read_bytes = recv(fd_, readBuffer_+read_idx, READ_BUFFER_SIZE-read_idx, 0);
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
        printf("%dusers: %s\n", fd_, readBuffer_);
    }
    return true;
}

void conn::process()
{
    
}
#include "conn.h"

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void ctl_addEvent(int eventfd_, int fd_, bool et){
    epoll_event event_;
    event_.events = EPOLLIN;
    event_.data.fd = fd_;
    if(et){
        event_.events |= EPOLLET;
    }
    epoll_ctl(eventfd_, EPOLL_CTL_ADD, fd_, &event_);
    setnonblocking(fd_);
}

int conn::m_epollfd = -1;

void conn::close_conn(){
    epoll_ctl(m_epollfd, EPOLL_CTL_DEL, fd_, 0);
    fd_ = -1;
}

void conn::init(int clientfd_)
{
    fd_ = clientfd_;
    read_idx = 0;

    //注册事件 ET
    ctl_addEvent(m_epollfd, clientfd_, true);

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
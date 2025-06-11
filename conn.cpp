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

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

int conn::m_epollfd = -1;
int conn::m_user_count = 0;

void conn::close_conn(){
    if(fd_ != -1)
    {
        removefd(m_epollfd, fd_);
        fd_ = -1;
        m_user_count--;
    }
}

void conn::init(int clientfd_)
{
    fd_ = clientfd_;

    //注册事件 ET
    ctl_addEvent(m_epollfd, clientfd_, true);
    m_user_count++;

    m_read_idx = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
}

bool conn::read(){
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    int read_bytes = 0;
    while(true)
    {
        read_bytes = recv(fd_, m_read_buf+m_read_idx, READ_BUFFER_SIZE-m_read_idx, 0);
        if(read_bytes == -1)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
            {
                break;
            }
        }
        else if(read_bytes == 0)
        {
            return false;
        }

        m_read_idx += read_bytes;
        printf("%dusers: %s\n", fd_, m_read_buf);
    }
    return true;
}

bool conn::write(){
    return false;
}

void conn::process()
{
    
}

/* 主状态机 */
conn::HTTP_CODE conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;

}

/* 从状态机 */
conn::LINE_STATUS conn::process_line()
{
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx)
    {
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r')
        {
            if((m_checked_idx+1) == m_read_idx)
            {
                return LINE_OPEN;
            }
            else if(m_read_buf[m_checked_idx+1] == '\n')
            {
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
        else if(temp == '\n')
        {
            if((m_checked_idx > 1) && (m_read_buf[m_checked_idx-1] == '\r'))
            {
                m_read_buf[m_checked_idx-1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}
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

    init();
}

void conn::init()
{
    m_read_idx = 0;
    m_checked_idx = 0;
    m_start_line = 0;
    m_content_length = 0;

    m_check_state = CHECK_STATE_REQUESTLINE;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_host = 0;
    m_linger = false;

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
        //printf("%dusers: %s\n", fd_, m_read_buf);
    }
    return true;
}

bool conn::write(){
    return false;
}

/********************************************************************************************/

void conn::process()
{
    HTTP_CODE read_ret = process_read();
    //printf("%s\n", read_ret);
}

/* 主状态机 */
conn::HTTP_CODE conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char* text = 0;

    while ((line_status = parse_line()) == LINE_OK)
    {
        text = get_line();
        m_start_line = m_checked_idx;
        printf("%s\n", text);

        switch (m_check_state)
        {
            case CHECK_STATE_REQUESTLINE:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }

            case CHECK_STATE_HEADER:
            {
                ret = parse_headers_line(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                else if(ret == GET_REQUEST)
                {
                    
                }
                
                break;
            }

            case CHECK_STATE_CONTENT:
            {
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST)
                {
                    return BAD_REQUEST;
                }
                break;
            }
            
            default:
            {
                return INTERNAL_ERROR;
            }
        }
    }
    return NO_REQUEST;
}

/* 从状态机 */
conn::LINE_STATUS conn::parse_line()
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

/* 解析http请求行 */
conn::HTTP_CODE conn::parse_request_line(char* text)
{
    m_url = strpbrk(text, " \t");
    if(!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0';

    char* method = text;
    if(strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else
    {
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if(!m_version)
    {
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");

    if(strcasecmp(m_version, "HTTP/1.1") != 0)
    {
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

/* 解析http头部行 */
conn::HTTP_CODE conn::parse_headers_line(char* text)
{
    if(text[0] == '\0')
    {
        if(m_content_length != 0)
        {
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if(strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else if(strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if(strncasecmp(text, "Content-Length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else
    {
        printf("oop! 不知道的header %s\n", text);
    }
    return NO_REQUEST;
}
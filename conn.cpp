#include "conn.h"

/* 网站根目录 */
const char* doc_root = "/var/www";
/* 定义HTTP响应的一些状态信息 */
const char* ok_200_title = "OK";
const char* error_400_title = "Bad Request";
const char* error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char* error_403_title = "Forbidden";
const char* error_403_form = "You do not have permission to get file from this server.\n";
const char* error_404_title = "Not Found";
const char* error_404_form = "The requested file was not found on this server.\n";
const char* error_500_title = "Internal Error";
const char* error_500_form = "There was an unusual problem serving the requested file.\n";

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

void modfd(int eventfd, int fd, int ev){
    epoll_event event_;
    event_.data.fd = fd;
    event_.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(eventfd, EPOLL_CTL_MOD, fd, &event_);
}

int conn::m_epollfd = -1;
int conn::m_user_count = 0;

/************************************************************************************************/

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

    m_write_idx = 0;

    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
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

/********************************************************************************************/

void conn::process()
{
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, fd_, EPOLLIN);
        return;
    }
    
    bool write_ret = process_write(read_ret);
    if(!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, fd_, EPOLLOUT);
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
                    return do_request();
                }
                break;
            }

            case CHECK_STATE_CONTENT:
            {
                ret = parse_content_line(text);
                if(ret == GET_REQUEST)
                {
                    return do_request();
                }
                line_status = LINE_OPEN;
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
    else if(strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
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

    if(strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/')
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
        //printf("oop! 不知道的header %s\n", text);
    }
    return NO_REQUEST;
}

/* 解析http头部行 */
conn::HTTP_CODE conn::parse_content_line(char* text)
{
    if(m_read_idx >= (m_checked_idx + m_content_length))
    {
        printf("post data: %s\n", text);
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/* 读取index.html */
conn::HTTP_CODE conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file+len, m_url, FILENAME_LEN-len-1);

    if(stat(m_real_file, &m_file_stat) < 0)
    {
        return NO_RESOURCE;
    }

    if(!(m_file_stat.st_mode & S_IROTH))
    {
        return FORBIDDEN_REQUEST;
    }

    if(S_ISDIR(m_file_stat.st_mode))
    {
        return BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/********************************************************************************************/

bool conn::write(){
    int temp = 0;
    int bytes_have_send = 0;
    int bytes_to_send = m_write_idx;
    if(bytes_to_send == 0)
    {
        modfd(m_epollfd, fd_, EPOLLIN);
        init();
        return true;
    }

    while(1)
    {
        //writev函数将多块分散的内存数据一并写入文件描述符中，即集中写
        //成功时返回读取fd的字节数，失败返回-1
        temp = writev(fd_, m_iv, m_iv_count);
        if(temp <= -1)
        {
            /* 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，
            虽然在此期间，服务器无法立即接收到同一客户的下一个请求，但这可以保证连接的完整性 */
            if(errno == EAGAIN)
            {
                modfd(m_epollfd, fd_, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if(bytes_to_send <= bytes_have_send)
        {
            /* 发送HTTP响应成功，根据HTTP请求中的Connection字段决定是否立即关闭连接 */
            unmap();
            if(m_linger)
            {
                init();
                modfd(m_epollfd, fd_, EPOLLIN);
                return true;
            }
            else
            {
                modfd(m_epollfd, fd_, EPOLLIN);
                return false;
            }
        }
    }
}

bool conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
        case FILE_REQUEST:
        {  
            add_status_line(200, ok_200_title);
            if(m_file_stat.st_size != 0)
            {
                add_headers(m_file_stat.st_size);
                m_iv[0].iov_base = m_write_buf;
                m_iv[0].iov_len = m_write_idx;
                m_iv[1].iov_base = m_file_address;
                m_iv[1].iov_len = m_file_stat.st_size;
                m_iv_count = 2;
                return true;
            }
            else
            {
                const char* ok_string = "<html><body></body></html>";
                add_headers(strlen(ok_string));
                if(!add_content(ok_string))
                {
                    return false;
                }
            }
        }

        case BAD_REQUEST:
        {
            add_status_line(400, error_400_title);
            add_headers(strlen(error_400_form));
            if(!add_content(error_400_form))
            {
                return false;
            }
            break;
        }

         case FORBIDDEN_REQUEST:
        {
            add_status_line(403, error_403_title);
            add_headers(strlen(error_403_form));
            if(!add_content(error_403_form))
            {
                return false;
            }
            break;
        }

        case NO_RESOURCE:
        {
            add_status_line(404, error_404_title);
            add_headers(strlen(error_404_form));
            if(!add_content(error_404_form))
            {
                return false;
            }
            break;
        }

        case INTERNAL_ERROR:
        {
            add_status_line(500, error_500_title);
            add_headers(strlen(error_500_form));
            if(!add_content(error_500_form))
            {
                return false;
            }
            break;
        }

        default:
        {
            return false;
        }
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    return true;
}

bool conn::add_response(const char* format, ...)
{
    if(m_write_idx >= WRITE_BUFFER_SIZE)
    {
        return false;
    }

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        return false;
    }

    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool conn::add_status_line(int status, const char* title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool conn::add_headers(int content_len)
{
    add_content_length(content_len);
    add_linger();
    add_blank_line();
}

bool conn::add_content(const char* content)
{
    return add_response("%s", content);
}

bool conn::add_content_length(int content_len)
{
    return add_response("Content-Length: %d\r\n", content_len);
}

bool conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}

void conn::unmap()
{
    if(m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}
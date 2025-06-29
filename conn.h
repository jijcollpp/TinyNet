#ifndef _CONN_H_
#define _CONN_H_
#include <unistd.h>
#include <errno.h>
#include <string.h> //bzero/memset
#include <stdio.h> //printf
#include <cstdlib> //atol

#include <sys/epoll.h>
#include <sys/socket.h> //socket/bind/listen/accept/recv
#include <fcntl.h> //setnonblocking

#include <sys/stat.h> //S_ISDIR
#include <sys/mman.h> //mmap

#include <stdarg.h> //va_start

class conn
{
public:
    //请求方法
    enum METHOD {GET = 0, POST, HEAD, PUT, DELETE, TRACE, OPTIONS, CONNECT, PATCH};
    ///主状态机
    enum CHECK_STATE {CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER, CHECK_STATE_CONTENT};
    //行读取状态
    enum LINE_STATUS {LINE_OK = 0, LINE_BAD, LINE_OPEN};
    //http请求结果
    enum HTTP_CODE {NO_REQUEST, GET_REQUEST, BAD_REQUEST,
                    NO_RESOURCE, FORBIDDEN_REQUEST, FILE_REQUEST,
                    INTERNAL_ERROR, CLOSED_CONNECTION};

    static const int READ_BUFFER_SIZE = 2048;
    static const int FILENAME_LEN = 200;
    static const int WRITE_BUFFER_SIZE = 1024;

public:
    conn(){};
    ~conn(){};

    void init(int clientfd_);
    bool read();
    bool write();
    void process();
    void close_conn();

private:
    void init();
    HTTP_CODE process_read();
    LINE_STATUS parse_line();
    HTTP_CODE parse_request_line(char* text);
    HTTP_CODE parse_headers_line(char* text);
    HTTP_CODE parse_content_line(char* text);
    HTTP_CODE do_request();
    char* get_line(){return m_read_buf + m_start_line;}

    bool process_write(HTTP_CODE ret);
    bool add_response(const char* format, ...);
    bool add_status_line(int status, const char* title);
    bool add_headers(int content_length);
    bool add_content(const char* content);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();
    void unmap();

public:
    static int m_epollfd;
    static int m_user_count;

private:
    int fd_;

    char m_read_buf[READ_BUFFER_SIZE]; //读缓冲区
    int m_read_idx; //读缓存区 尾标识符
    int m_checked_idx; //从状态机 每一行行首 读取标识符
    int m_start_line; //解析行的起始标识符
    int m_content_length; //消息体长度

    CHECK_STATE m_check_state;
    METHOD m_method;
    char* m_version;
    char* m_url; //访问的index.html文件
    char* m_host;
    int m_linger; //socket是否保持连接

    char m_write_buf[WRITE_BUFFER_SIZE];
    int m_write_idx;

    char m_real_file[FILENAME_LEN]; //文件地址
    struct stat m_file_stat;
    char* m_file_address;
    struct iovec m_iv[2];
    int m_iv_count;
};

#endif
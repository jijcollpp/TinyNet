#include <stdio.h> //printf
#include <libgen.h> //basename
#include <assert.h> //assert
#include <stdlib.h> //atoi
#include <string.h> //bzero/memset

#include <sys/types.h>
#include <sys/socket.h> //socket/bind/listen/accept/recv
#include <netinet/in.h> //ipv4
#include <unistd.h> //close
#include <sys/epoll.h>
#include <arpa/inet.h> //

#include "threadPool.h"
#include "conn.h"

#define MAX_EVENT_NUMBER 10000
#define MAX_FD 65536

extern int ctl_addEvent(int epollfd, int fd, bool et);

int main(int argc, char* argv[])
{
    if(argc <= 2){
        printf("usage: %s ip_adress port_number\n", basename(argv[0]));
        return 0;
    }
    char* ip = argv[1];
    int port = atoi(argv[2]);

    //线程池
    threadPool<conn> *pool = NULL;
    try
    {
        pool = new threadPool<conn>;
    }
    catch(...)
    {
        return 1;
    }

    //创建socket
    int listenfd_ = socket(AF_INET, SOCK_STREAM, 0);
    assert(listenfd_ != -1);

    int ret = 0;
    //命名socket
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);
    ret = bind(listenfd_, (struct sockaddr*)&server_addr, sizeof(server_addr));
    assert(ret != -1);

    //监听socket
    ret = listen(listenfd_, 5);
    assert(ret != -1);

    //epoll
    int epollfd_ = epoll_create(5);
    assert(epollfd_ != -1);
    ctl_addEvent(epollfd_, listenfd_, false);
    conn::m_epollfd = epollfd_;

    epoll_event ready_event[MAX_EVENT_NUMBER];
    conn* client_arr = new conn[MAX_FD];

    while(true){
        int number = epoll_wait(epollfd_, ready_event, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR))
        {
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; i++)
        {
            int sockfd = ready_event[i].data.fd;
            if(sockfd == listenfd_)
            {
                //接受连接
                struct sockaddr_in client_addr;
                socklen_t client_socklent = sizeof(client_socklent);
                int clientfd_ = accept(listenfd_, (struct sockaddr*)&client_addr, &client_socklent);
                if(clientfd_ < 0)
                {
                    printf("errno is: %d\n", errno);
                }
                if(conn::m_user_count >= MAX_FD)
                {
                    printf("Internal server busy\n");
                    send(clientfd_, "Internal server busy\n", strlen("Internal server busy\n"), 0);
                    close(clientfd_);
                    continue;
                }

                client_arr[clientfd_].init(clientfd_);
                printf("new client fd %d! IP: %s Port: %d\n", clientfd_, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
            else if(ready_event[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR))
            {
                client_arr[sockfd].close_conn();
            }
            else if(ready_event[i].events & EPOLLIN) //可读
            {
                if(client_arr[sockfd].read())
                {
                    //添加到线程池
                    pool->append(client_arr + sockfd);
                }
                else
                {
                    client_arr[sockfd].close_conn();
                }
            }
            else if(ready_event[i].events & EPOLLOUT) //可写
            {
                if(!client_arr[sockfd].write())
                {
                    client_arr[sockfd].close_conn();
                }
            }
            else
            {}
        }
    }

    close(epollfd_);
    close(listenfd_);
    delete [] client_arr;
    delete pool;
    return 0;
}

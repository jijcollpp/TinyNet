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
#include <fcntl.h>
#include <arpa/inet.h> //

#define MAX_EVENT_NUMBER 10000
#define BUFFER_SIZE 1024

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

epoll_event ctl_addEvent(int eventfd_, int fd_, bool et){
    epoll_event event_;
    event_.events = EPOLLIN;
    event_.data.fd = fd_;
    if(et){
        event_.events |= EPOLLET;
    }
    epoll_ctl(eventfd_, EPOLL_CTL_ADD, fd_, &event_);
    setnonblocking(fd_);
}

int main(int argc, char* argv[])
{
    if(argc <= 2){
        printf("usage: %s ip_adress port_number\n", basename(argv[0]));
        return 0;
    }
    char* ip = argv[1];
    int port = atoi(argv[2]);

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

    epoll_event ready_event[MAX_EVENT_NUMBER];
    //int client_event[MAX_EVENT_NUMBER];

    while(true){
        int number = epoll_wait(epollfd_, ready_event, MAX_EVENT_NUMBER, -1);
        if(number < 0){
            printf("epoll failure\n");
            break;
        }

        char recvBuffer_[BUFFER_SIZE];
        for(int i = 0; i < number; i++){
            if(ready_event[i].data.fd == listenfd_){
                //接受连接
                struct sockaddr_in client_addr;
                socklen_t client_socklent = sizeof(client_socklent);
                int clientfd_ = accept(listenfd_, (struct sockaddr*)&client_addr, &client_socklent);
                assert(clientfd_ != -1);

                ctl_addEvent(epollfd_, clientfd_, true);
                //client_event[clientfd_] = clientfd_;

                printf("new client fd %d! IP: %s Port: %d\n", 
                        clientfd_, inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            }
            else if(ready_event[i].events & EPOLLIN){
                int client = ready_event[i].data.fd;

                memset(recvBuffer_, '\0', BUFFER_SIZE);
                int read_bytes = recv(client, recvBuffer_, sizeof(recvBuffer_), 0);
                if(read_bytes > 0){
                    printf("%dusers: %s\n", client, recvBuffer_);
                }else if(ret <= 0)
                {
                    close(client);
                    break;
                }
                
            }
        }
    }
    
    // TODO:
        //123

    close(epollfd_);
    close(listenfd_);
    return 0;
}
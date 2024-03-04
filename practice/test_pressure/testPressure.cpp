#include<stdlib.h>
#include<stdio.h>
#include<assert.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/epoll.h>
#include<fcntl.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>
#include<errno.h>

#include "../src/support.h"

static const char * request = "GET http://localhost/hello.txt HTTP/1.0\r\nConnection:keep-alive\r\n\r\n";
static int success_times = 0;
static int fail_times = 0;

void addfd_out(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLOUT | EPOLLET | EPOLLERR;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

bool write_nbytes(int sockfd, const char * buffer, int len){
    int bytes_write = 0;
    printf("write out %d bytes to socket %d\n", len, sockfd);
    while (true)
    {
        bytes_write = send(sockfd, buffer, len, 0);
        if(bytes_write == -1){
            if((errno != EAGAIN) && (errno != EWOULDBLOCK))
                return false;
        }else if(bytes_write == 0){
            return false;
        }
        len -= bytes_write;
        buffer = buffer + bytes_write;
        if(len <= 0){
            return true;
        }
    }
}

bool read_once(int sockfd, char * buffer, int len){
    int bytes_read = 0;
    memset(buffer, '\0', len);
    while (true)
    {
        bytes_read = recv(sockfd, buffer, len, 0);
        if(bytes_read < 0){
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                break;
            }else{
                return false;
            }
            
        }else if(bytes_read == 0){
            return false;
        }
    }
    printf("read in %d bytes from socket %d with content: %s\n", bytes_read, sockfd, buffer);
    success_times++;
    return true;
}

void start_conn(int epollfd, int num, const char * ip, const char * port){
    int sockfd;
    for(int i = 0; i < num; ++i){
        sleep(1);
        if((sockfd = open_clientfd(ip, port)) >= 0){
            printf("build connection %d\n", sockfd);
            addfd_out(epollfd, sockfd);
        }
    }
}

int main(int argc, char * argv[]){
    assert(argc == 4);
    int epoll_fd = epoll_create(100);
    start_conn(epoll_fd, atoi(argv[3]), argv[1], argv[2]);
    epoll_event events[10000];
    char buffer[2048];
    while(1){
        int fds = epoll_wait(epoll_fd, events, 10000, 2000);
        for(int i = 0; i < fds; ++i){
            int sockfd = events[i].data.fd;
            if(events[i].events & EPOLLIN){
                if(!read_once(sockfd, buffer, 2048)){
                    printf("read remove %d\n", sockfd);
                    fail_times++;
                    removefd(epoll_fd, sockfd);
                }
                struct epoll_event event;
                event.events = EPOLLOUT | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
            }else if(events[i].events & EPOLLOUT){
                if(!write_nbytes(sockfd, request, strlen(request)) || !write_nbytes(sockfd, request, strlen(request))){
                    printf("write remove %d\n", sockfd);
                    fail_times++;
                    removefd(epoll_fd, sockfd);
                }
                struct epoll_event event;
                event.events = EPOLLIN | EPOLLET | EPOLLERR;
                event.data.fd = sockfd;
                epoll_ctl(epoll_fd, EPOLL_CTL_MOD, sockfd, &event);
            }else if(events[i].events & EPOLLERR){
                fail_times++;
                removefd(epoll_fd, sockfd);
            }
        }
        printf("success times : %d\n", success_times);
        printf("fail times : %d\n", fail_times);
    }
}
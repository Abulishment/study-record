#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<pthread.h>
#include "../src/support.h"

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 10

void lt(epoll_event * events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; ++i){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            struct sockaddr_storage client_address;
            socklen_t client_len;
            int connfd = accept(listenfd, (sockaddr * ) &client_address, &client_len);
            addfd_et(epollfd, connfd, false);
        }else if(events[i].events & EPOLLIN){
            printf("event trigger once\n");
            memset(buf, '\0', BUFFER_SIZE);
            int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            if(ret <= 0){
                close(sockfd);
                continue;
            }
            printf("get %d bytes of content: %s\n", ret, buf);
        }else{
            printf("something else happened\n");
        }
    }
}

void et(epoll_event * events, int number, int epollfd, int listenfd){
    char buf[BUFFER_SIZE];
    for(int i = 0; i < number; ++i){
        int sockfd = events[i].data.fd;
        if(sockfd == listenfd){
            struct sockaddr_storage client_address;
            socklen_t client_len;
            int connfd = accept(listenfd, (struct sockaddr * ) &client_address, & client_len);
            addfd_et(epollfd, connfd, true);
        }else if(events[i].events & EPOLLIN){
            printf("event trigger once\n");
            while(true){
                memset(buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
                if(ret < 0){
                    if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                        printf("read later\n");
                        break;
                    }
                    printf("recv in et error\n");
                    close(sockfd);
                    break;
                }else if(ret == 0){
                    close(sockfd);
                }else{
                    printf("get %d bytes of content: %s\n", ret, buf);
                }
            }
        }else{
            printf("something else happened\n");
        }
    }
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage:%s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }
    const char *ip = argv[1];
    const char *port = argv[2];

    int epollfd, listenfd;

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "can't open clientfd %s:%s\n", ip, port);
        return 1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    if((epollfd = epoll_create(5)) < 0){
        fprintf(stderr, "can't create epollfd\n");
        return 1;
    }
    addfd_et(epollfd, listenfd, true);

    while(true){
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0){
            fprintf(stderr, "epoll failure\n");
            break;
        }
        // lt(events, ret, epollfd, listenfd);
        et(events, ret, epollfd, listenfd);
    }
    close(listenfd);
    return 0;
}
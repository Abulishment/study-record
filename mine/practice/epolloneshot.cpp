#include "../src/support.h"
#include<sys/types.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<errno.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<pthread.h>
#include<stdio.h>
#include <string.h>
#include "../../../../../usr/include/x86_64-linux-gnu/sys/socket.h"
#include <unistd.h>

#define MAX_EVENT_NUMBER 1024
#define BUFFER_SIZE 1024

struct fds{
    int epollfd;
    int sockfd;
};

void *worker(void *arg){
    int sockfd = ((fds*) arg)->sockfd;
    int epollfd = ((fds*) arg)->epollfd;
    printf("start new thread to receive data on fd: %d\n", sockfd);
    char buf[BUFFER_SIZE];
    memset(buf, '\0', BUFFER_SIZE);
    while(true){
        int ret = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
        if(ret == 0){
            close(sockfd);
            printf("foreigner closed the connection\n");
            break;
        }else if(ret < 0){
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                reset_oneshot(epollfd, sockfd);
                printf("read later\n");
                break;
            }
            printf("error in recv\n");
            close(sockfd);
            break;
        }else{
            printf("getcontent: %s\n", buf);
            sleep(5);
        }
    }
    printf("end thread receiving data on fd: %d\n", sockfd);
    return nullptr;
}

int main(int argc, char *argv[]){
    if(argc <= 2){
       fprintf(stderr, "usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    int epollfd,listenfd; 
    epoll_event events[MAX_EVENT_NUMBER];
    const char *ip = argv[1];
    const char *port = argv[2];
    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "open_listenfd error\n");        
        return 1;
    }

    if((epollfd = epoll_create(5)) < 0){
        fprintf(stderr, "epoll_create error\n");
        return 1;
    } 
    addfd_oneshot(epollfd, listenfd, false);

    while(true){
        int ret = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(ret < 0){
            fprintf(stderr, "epoll failure\n");
            break;
        }
        for(int i = 0; i < ret; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                struct sockaddr_storage client_address;
                socklen_t client_len;
                int connfd = accept(listenfd, (sockaddr *) &client_address, &client_len);
                addfd_oneshot(epollfd, connfd, true);
            }else if(events[i].events & EPOLLIN){
                pthread_t thread;
                fds fds_for_new_worker;
                fds_for_new_worker.epollfd = epollfd;
                fds_for_new_worker.sockfd = sockfd;
                pthread_create(&thread, NULL, worker, (void *) & fds_for_new_worker);
            }else{
                printf("something else happened\n");
            }
        }
    }

    close(listenfd);
    return 0;
}
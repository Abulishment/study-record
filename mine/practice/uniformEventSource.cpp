#include "../src/support.h"
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<signal.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<sys/epoll.h>
#include<pthread.h>

#define MAX_EVENT_NUMBER 1024

static int pipefd[2];

void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1], (char*) & msg, 1, 0);
    errno = save_errno;
}

void addsig(int sig){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    int listenfd,epollfd;
    const char *ip = argv[1];
    const char *port = argv[2];
    bool stop_server = false;

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "open_listenfd fails\n");
        return 1;
    } 

    if((epollfd = epoll_create(5)) < 0){
        fprintf(stderr, "epoll_create fails\n");
        return 1;
    }

    addfd_et(epollfd, listenfd, true);
    if(socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) == -1){
        fprintf(stderr, "socketpair fails\n");
        return 1;
    }
    setnonblocking(pipefd[1]);
    addfd_et(epollfd, pipefd[0], true);
    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    while(!stop_server){
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0 && (errno != EINTR)){
            printf("epoll failure\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;
            if((sockfd == listenfd) && (events[i].events & EPOLLIN)){
                struct sockaddr_storage client_address;
                socklen_t clientlen;
                int clientfd;
                if((clientfd = accept(listenfd, (sockaddr*) &client_address, &clientlen)) < 0){
                    fprintf(stderr, "accept from %d error\n", listenfd);
                }else{
                    addfd_et(epollfd, clientfd, true);
                } 
            }else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                int ret = recv(sockfd, signals, sizeof(signals), 0);
                if(ret == -1){
                    if((errno != EAGAIN) && (errno != EWOULDBLOCK)){
                        printf("recv from pipefd[0] fails\n");
                    }
                }else if(ret == 0){
                    printf("pipefd[1] closed by the other side\n");
                }else{
                    for(int j = 0; j < ret; ++j){
                        switch(signals[j]){
                            case SIGCHLD:
                            case SIGHUP:
                                continue;
                            case SIGTERM:
                            case SIGINT:
                                stop_server = true;
                        }
                    }
                }
            }else{
                printf("clientfd data comes(ignore simply in this program) or unexpected event happens\n");
            }
        }
    }

    printf("close fds\n");
    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    close(epollfd);
    return 0;
}
#include "../src/support.h"
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<poll.h>
#include<fcntl.h>
#include<errno.h>

#define MAX_EVENT_NUMBER 1024
#define TCP_BUFFER_SIZE 512
#define UDP_BUFFER_SIZE 1024

int main(int argc, char *argv[]){
    if(argc <= 2){
        fprintf(stderr, "usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    int listenfd, udpfd, epollfd;
    const char *ip = argv[1];
    const char *port = argv[2];

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "open_listenfd error\n");
        return 1;
    }

    if((udpfd = open_listenfd(port, false)) < 0){
        fprintf(stderr, "open_listenfd udp error\n");
        return 1;
    }

    struct epoll_event events[MAX_EVENT_NUMBER];
    assert((epollfd = epoll_create(5)) != -1);
    addfd_et(epollfd, listenfd, true);
    addfd_et(epollfd, udpfd, true);

    while(true){
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if(number < 0){
            fprintf(stderr, "epoll_wait error\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;

            if(sockfd == listenfd){
                struct sockaddr_storage client_address;
                socklen_t client_len;
                int clientfd;
                if((clientfd = accept(sockfd, (sockaddr*) &client_address, &client_len)) < 0){
                    fprintf(stderr, "accept connection to %d fail : %s\n", listenfd, strerror(errno));
                    continue;
                }
                addfd_et(epollfd, clientfd, true);
            }else if(sockfd == udpfd){
                char buf[UDP_BUFFER_SIZE];
                struct sockaddr_storage client_address;
                socklen_t client_len;
                int ret = recvfrom(sockfd, buf, UDP_BUFFER_SIZE - 1, 0, (sockaddr*) &client_address, &client_len);
                if(ret > 0){
                    sendto(udpfd, buf, strlen(buf), 0, (sockaddr*) &client_address, client_len);
                }else{
                    if((errno == EAGAIN || errno == EWOULDBLOCK)){
                        printf("udp read nothing\n");
                    }else{
                        printf("udp read error\n");
                    }
                }
            }else if(events[i].events & EPOLLIN){
                char buf[TCP_BUFFER_SIZE];
                while(true){
                    memset(buf, '\0', TCP_BUFFER_SIZE);
                    int ret = recv(sockfd, buf, TCP_BUFFER_SIZE - 1, 0);
                    if(ret < 0){
                        if((errno != EAGAIN) && (errno != EWOULDBLOCK)){
                            close(sockfd);
                        }
                        break;
                    }else if(ret == 0){
                        close(sockfd);
                        break;
                    }else{
                        send(sockfd, buf, ret, 0);
                    }
                }
            }else{
                printf("something else happened\n");
            }
        }
    }

    close(listenfd);
    close(udpfd);
    close(epollfd);
    return 0;
}
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<cassert>
#include<sys/epoll.h>
#include<iostream>

// #include "locker.h"
#include "./threadpool/threadPool.h"
#include "./http/httpConnection.h"
#include "./src/support.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000

void addsig(int sig, void(*handler)(int), bool restart){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void send_error(int connfd, const char * info){
    printf("%s", info);
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        fprintf(stderr, "usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    addsig(SIGPIPE, SIG_IGN);
    
    ThreadPool<Http_Conn> * pool = NULL;
    try
    {
        pool = new ThreadPool<Http_Conn>;
    }
    catch(const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }

    Http_Conn * users = new Http_Conn[MAX_FD];
    assert(users);
    int user_count = 0;

    int listenfd, epollfd;
    const char * ip = argv[1];
    const char * port = argv[2];
    epoll_event events[MAX_EVENT_NUMBER];

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "open_listenfd(%s) fails", port);
        return 1;
    }

    assert((epollfd = epoll_create(5)) >= 0);
    addfd_oneshot(epollfd, listenfd, false);
    Http_Conn::m_epollfd = epollfd;

    while(true){
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR)){
            fprintf(stderr, "epoll_wait fails\n");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;

            if(sockfd == listenfd){
                struct sockaddr_storage client_address;
                socklen_t client_len;
                int clientfd;
                clientfd = accept(listenfd, (sockaddr*) &client_address, &client_len);
                if(clientfd < 0){
                    fprintf(stderr, "accept fails : %s\n", strerror(errno));
                    continue;
                }
                if(Http_Conn::m_user_count >= MAX_FD){
                    send_error(clientfd, "Internal server busy\n");
                    continue;
                }
                printf("accept connection %d(fd)\n", clientfd);
                users[clientfd].init(clientfd, client_address, client_len);
            }else if(events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)){
                printf("connection closed by foreigner or error: ");
                users[sockfd].close_conn();
            }else if(events[i].events & EPOLLIN){
                if(users[sockfd].read()){
                    pool->append(users + sockfd); 
                }else{
                    printf("read error: ");
                    users[sockfd].close_conn();
                }
            }else if(events[i].events & EPOLLOUT){
                if(!users[sockfd].write()){
                    printf("write error: ");
                    users[sockfd].close_conn();
                }
            }else{
                printf("unexpected event happens\n");
            }
        }
    }

    close(epollfd);
    close(listenfd);
    delete[] users;
    delete pool;
    return 0;
}
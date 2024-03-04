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
#include<vector>

#include "./lock/locker.h"
#include "./threadpool/threadPool.h"
#include "./http/httpConnection.h"
#include "./src/support.h"
#include "./log/logger.h"

#define MAX_FD 65536
#define MAX_EVENT_NUMBER 10000
#define TIME_SLOT 1

static bool print = true;

class WebServer{
public:
    WebServer(const char* port){
        strcpy(this->port, port);
        users = new Http_Conn[MAX_FD];
    } 

    virtual ~WebServer(){
        if(epoll_fd >= 0)
            close(epoll_fd);
        if(listen_fd >= 0)
            close(listen_fd);
        delete[] users;
    }

    void init(){
        epoll_fd = epoll_create(100);
        if(epoll_fd < 0)
            throw std::exception();
        listen_fd = open_listenfd(port);
        if(listen_fd < 0)
            throw std::exception();

        addfd(epoll_fd, listen_fd, false, false);
        user_count = 0;
        Http_Conn::m_epollfd = epoll_fd;
        init_signal();
    }

    void deal_connection(int sockfd){
        struct sockaddr_storage client_address;
        socklen_t client_len = sizeof(client_address);
        int clientfd;
        clientfd = accept(listen_fd, (sockaddr*) &client_address, &client_len);
        if(clientfd < 0){
            // if(print)
            // printf("accept fails : %s clientfd is %d, listen_fd is %d\n", strerror(errno), clientfd, listen_fd);
            LOG(std::string("accept fails : ") + strerror(errno) + " clientfd is " + std::to_string(clientfd) + ", listenfd is " + std::to_string(listen_fd));
        }else if(Http_Conn::m_user_count >= MAX_FD){
            // if(print)
            // printf("too many users. refuse connection\n"); 
            LOG(std::string("too many users. refuse connection\n"));
            send_error(clientfd, "Internal server busy\n");
        }else{
            // if(print)
            // printf("accept connection %d(fd)\n", clientfd);
            LOG(std::string("accept connection : ") + std::to_string(clientfd) + "\n");
            users[clientfd].init(clientfd, client_address, client_len);
        }
    }

    void deal_err(int sockfd){
            // if(print)
        // printf("%d connection error: %s\n", sockfd, strerror(errno));
        LOG(std::to_string(sockfd) + " connection error: " + strerror(errno) + '\n');
        users[sockfd].close_conn();
    }

    void deal_close(int sockfd){
        //     if(print)
        // printf("%d connection closed by foreign host", sockfd); 
        LOG(std::to_string(sockfd) + " connection closed by foreign host:" + '\n');
        users[sockfd].close_conn();
    }

    void deal_read(int sockfd){
        if(users[sockfd].read()){
            // if(print)
            // printf("read success in %d\n", sockfd);
            LOG(std::string("read success in ") + std::to_string(sockfd) + '\n');
            pool.append(users + sockfd); 
        }else{
            users[sockfd].close_conn();
        }
    }

    void deal_write(int sockfd){
        if(!users[sockfd].write()){
            users[sockfd].close_conn();
        }
    }

    void eventloop(){
        // if(print)
        // printf("listenfd = %d, epollfd = %d\n", listen_fd, epoll_fd);
        LOG(std::string("listenfd = ") + std::to_string(listen_fd) + ", epollfd = " + std::to_string(epoll_fd) + '\n');
        while(true){
            int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
            if((number < 0) && (errno != EINTR)){
                // fprintf(stderr, "epoll_wait fails\n");
                LOG(std::string("epoll_wait errors\n"));
                break;
            }

            for(int i = 0; i < number; ++i){
                int sockfd = events[i].data.fd;
                if(sockfd == listen_fd){
                    deal_connection(sockfd);
                }else if(events[i].events & (EPOLLHUP | EPOLLERR)){
                    deal_err(sockfd);
                }else if(events[i].events & EPOLLRDHUP){
                    deal_close(sockfd);
                }else if(events[i].events & EPOLLIN){
                    deal_read(sockfd);
                }else if(events[i].events & EPOLLOUT){
                    deal_write(sockfd);
                }else{
                    // printf("unexpected event happens\n");
                    LOG(std::string("unexpected event happens\n"));
                }
            }
        }
    }

    void init_signal(){
        signal(SIGPIPE, SIG_IGN);
    }
private:
    ThreadPool<Http_Conn> pool;
    Http_Conn* users;
    epoll_event events[MAX_EVENT_NUMBER];
    int user_count = 0;
    int epoll_fd = -1;
    int listen_fd = -1;
    char port[10];
};

int main(int argc, char *argv[]){
    if(argc <= 1){
        fprintf(stderr, "usage : %s [port_number]\n", basename(argv[0]));
        return 1;
    }
    const char * port = argv[1];

    Logger::getInstance()->init("./log.txt");
    WebServer ws(port);
    ws.init();
    ws.eventloop();
    return 0;
}
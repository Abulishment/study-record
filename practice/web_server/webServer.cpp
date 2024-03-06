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

#include "webServer.h"

#define MAX_FD 65536
#define TIME_SLOT 1

static bool print = true;
static int sig_pipe[2];

WebServer::WebServer(){
    users = new Http_Conn[MAX_FD];
    pool = new ThreadPool<Http_Conn>();
} 

WebServer::~WebServer(){
    if(epoll_fd >= 0)
        close(epoll_fd);
    if(listen_fd >= 0)
        close(listen_fd);
    delete[] users;
    delete pool;
}

void WebServer::init(const char* port, bool set_reactor_mode){
    epoll_fd = epoll_create(100);
    if(epoll_fd < 0)
        throw std::exception();
    listen_fd = open_listenfd(port);
    if(listen_fd < 0)
        throw std::exception();
    addfd(epoll_fd, listen_fd, false, false);

    user_count = 0;
    if(set_reactor_mode){
        Http_Conn::actor_mode = 0;
    }else{
        Http_Conn::actor_mode = 1;
    }
    Http_Conn::m_epollfd = epoll_fd;
    Http_Conn::lst = &lst;

    assert(socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipe) == 0);
    init_signal();
    addfd(epoll_fd, sig_pipe[0], false, false);
    alarm(TIME_SLOT);
}

void WebServer::timer_cb_func(Http_Conn * con_data){
    con_data->timer_func();
    con_data->timer = NULL;
}

void WebServer::deal_connection(){
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

        util_timer<Http_Conn> *timer = new util_timer<Http_Conn>();
        timer->user_data = users + clientfd;
        timer->absolute_expire = time(NULL) + 3 * TIME_SLOT;
        timer->cb_func = timer_cb_func;
        users[clientfd].timer = timer;
        lst.add_timer(timer);
    }
}

void WebServer::deal_err(int sockfd){
        // if(print)
    // printf("%d connection error: %s\n", sockfd, strerror(errno));
    LOG(std::to_string(sockfd) + " connection error: " + strerror(errno) + '\n');
    users[sockfd].close_conn();
}

void WebServer::deal_close(int sockfd){
    //     if(print)
    // printf("%d connection closed by foreign host", sockfd); 
    LOG(std::to_string(sockfd) + " connection closed by foreign host:" + '\n');
    users[sockfd].close_conn();
}

void WebServer::timer_update(int sockfd){
    Http_Conn* hp = &users[sockfd];
    if(!hp->timer){
        return;
    }
    hp->timer->absolute_expire = time(NULL) + 3 * TIME_SLOT;
    lst.adjust_timer(users[sockfd].timer);
}

void WebServer::deal_read(int sockfd){
    timer_update(sockfd);

    if(actor_mode == 0){
        while(!pool->append(users + sockfd, 0));
        return;
    }
    if(users[sockfd].read()){
        // if(print)
        // printf("read success in %d\n", sockfd);
        LOG(std::string("read success in ") + std::to_string(sockfd) + '\n');
        while(!pool->append(users + sockfd)); 
    }else{
        users[sockfd].close_conn();
    }
}

void WebServer::deal_write(int sockfd){
    timer_update(sockfd);
    if(actor_mode == 0){
        while(!pool->append(users + sockfd, 1));
        return;
    }
    if(!users[sockfd].write()){
        users[sockfd].close_conn();
    }
}

void WebServer::eventloop(){
    // if(print)
    // printf("listenfd = %d, epollfd = %d\n", listen_fd, epoll_fd);
    LOG(std::string("listenfd = ") + std::to_string(listen_fd) + ", epollfd = " + std::to_string(epoll_fd) + '\n');

    bool timeout = false;
    bool stop_server = false;
    while(!stop_server){
        int number = epoll_wait(epoll_fd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR)){
            // fprintf(stderr, "epoll_wait fails\n");
            LOG(std::string("epoll_wait errors\n"));
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listen_fd){
                deal_connection();
            }else if((sockfd == sig_pipe[0]) && (events[i].events & EPOLLIN)){
                if(!deal_signal(timeout, stop_server)){
                    LOG(std::string("deal_signal fails\n"));
                }
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

        if(timeout){
            LOG("timer tick\n");
            lst.tick();
            timeout = false;
            alarm(TIME_SLOT);
        }
    }
}

bool WebServer::deal_signal(bool & timeout, bool & stop_server){
    int ret;
    char signal[1024]; 
    ret = recv(sig_pipe[0], signal, sizeof(signal), 0);
    if(ret <= 0){
        return false;
    }

    for(int i = 0; i < ret; ++i){
        switch (signal[i])
        {
        case SIGALRM:
            timeout = true; 
            break;
        case SIGINT:
            stop_server = true; 
            break;
        case SIGTERM:
            stop_server = true;
            break;
        default:
            break;
        }
    }
    return true;
}

void WebServer::init_signal(){
    addsig(SIGALRM, signal_handler);
    addsig(SIGPIPE, SIG_IGN);
}

void WebServer::signal_handler(int sig){
    int saved_errno = errno;
    int msg = sig;
    send(sig_pipe[1], (char *) &msg, 1, 0);
    errno = saved_errno;
}

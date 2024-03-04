#include "support.h"
#include<sys/socket.h>
#include<stdio.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<stdlib.h>
#include<stdio.h>
#include<netdb.h>
#include<sys/types.h>
#include<errno.h>
#include <string.h>
#include <fcntl.h>
#include<signal.h>
#include "../../../../../usr/include/x86_64-linux-gnu/sys/epoll.h"
#include <cassert>

#define LISTENQ 5

/*声明中指定了默认值，则定义中不可重复指定*/
int open_listenfd(const char *port, bool isTcp){
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    memset(&hints, 0, sizeof(struct addrinfo));
    if(isTcp){
        hints.ai_socktype = SOCK_STREAM;
    }else{
        hints.ai_socktype = SOCK_DGRAM;
    }
    hints.ai_flags = AI_PASSIVE | AI_ADDRCONFIG;
    hints.ai_flags |= AI_NUMERICSERV;
    if((rc = getaddrinfo(NULL, port, &hints, &listp)) != 0){
        fprintf(stderr, "getaddrinfo failed (port %s): %s\n", port, gai_strerror(rc));
        return -2;
    }

    for(p = listp; p; p = p->ai_next){
        if((listenfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0){
            continue;
        }
        setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const void *)&optval, sizeof(int));

        if(bind(listenfd, p->ai_addr, p->ai_addrlen) == 0){
            break;
        }
        if(close(listenfd < 0)){
            fprintf(stderr, "open_listenfd close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    freeaddrinfo(listp);
    if(!p){
        fprintf(stderr, "no fitable local address\n");
        return -1;
    }
    if(isTcp && listen(listenfd, LISTENQ) < 0){
        fprintf(stderr, "can't listen\n");
        close(listenfd);
        return -1;
    }
    return listenfd;
}

int open_clientfd(const char *hostname, const char *port, bool isTcp){
    int clientfd, rc;
    struct addrinfo hints, *listp, *p;

    memset(&hints, 0, sizeof(struct addrinfo));
    if(isTcp){
        hints.ai_socktype = SOCK_STREAM;
    }else{
        hints.ai_socktype = SOCK_DGRAM;
    }
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_flags |= AI_ADDRCONFIG;
    if((rc = getaddrinfo(hostname, port, &hints, &listp)) != 0){
        fprintf(stderr, "getaddrinfo failed (%s:%s): %s\n", hostname, port, gai_strerror(rc));
    }

    for(p = listp; p; p = p->ai_next){
        if((clientfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) < 0){
            continue;
        }

        if(!isTcp){
            break;
        }

        if(connect(clientfd, p->ai_addr, p->ai_addrlen) != -1){
            break;
        }

        if(close(clientfd) < 0){
            fprintf(stderr, "open_clientfd: close failed: %s\n", strerror(errno));
            return -1;
        }
    }

    freeaddrinfo(listp);
    if(!p){
        return -1;
    }else{
        return clientfd;
    }
}

int setnonblocking(int fd){
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

void addfd(int epollfd, int fd, bool enable_et, bool enable_oneshot){
    epoll_event event;
    event.data.fd = fd;
    if(enable_et){
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    }else{
        event.events = EPOLLIN | EPOLLRDHUP;
    }
    if(enable_oneshot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void modfd(int epollfd, int fd, int ev, bool enable_et){
    epoll_event event;
    event.data.fd = fd;
    if(enable_et){
        event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    }else{
        event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
    }
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

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
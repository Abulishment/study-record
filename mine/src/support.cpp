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

void addfd_et(int epollfd, int fd, bool enable_et){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if(enable_et){
        event.events |= EPOLLET;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void addfd_oneshot(int epollfd, int fd, bool oneshot){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
    if(oneshot){
        event.events |= EPOLLONESHOT;
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

void reset_oneshot(int epollfd, int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;
    if(epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event) < 0){
        printf("epoll_ctl in reset_oneshot error\n");
    }
}

void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

void removefd(int epollfd, int fd){
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}
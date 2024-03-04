#include<stdio.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <cerrno>

int open_listenfd(const char *port){
    struct addrinfo hints, *listp, *p;
    int listenfd, rc, optval = 1;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_socktype = SOCK_STREAM;
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
        char host[30];
        char service[30];
        memset(host, '\0', 30);
        memset(service, '\0', 30);
        getnameinfo((sockaddr * )&p->ai_addr, p->ai_addrlen, host, 30, service, 30, 0);
        printf("%s:%s\n", host, service);
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
    if(listen(listenfd, 5) < 0){
        fprintf(stderr, "can't listen\n");
        close(listenfd);
        return -1;
    }
    return listenfd;
}

int main(){
    open_listenfd("8");
    return 0;
}
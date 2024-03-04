#define _GNU_SOURCE 1
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<poll.h>
#include<fcntl.h>
#include"../src/support.h"
#define BUFFER_SIZE 64

int main(int argc, char *argv[]){
    if(argc <= 2){
        fprintf(stderr, "usage: %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    const char *ip = argv[1];
    const char *port = argv[2];
    int client_fd, ret;
    char read_buf[BUFFER_SIZE];
    pollfd fds[2];
    int pipefd[2];

    if((client_fd = open_clientfd(ip, port)) < 0){
        fprintf(stderr, "can't open_clientfd for %s:%s\n", ip, port);
        return 1;
    }

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;
    fds[0].revents = 0;
    fds[1].fd = client_fd;
    fds[1].events = POLLIN | POLLRDHUP;
    fds[1].revents = 0;
    if((ret = pipe(pipefd)) < 0){
        fprintf(stderr, "pipe error\n");
    }

    while(true){
        ret = poll(fds, 2, -1);
        if(ret < 0){
            fprintf(stderr, "poll failure\n");
            break;
        }

        if(fds[1].revents & POLLRDHUP){
            printf("server close the connection\n");
            break;
        }else if(fds[1].revents & POLLIN){
            memset(read_buf, '\0', BUFFER_SIZE);
            recv(fds[1].fd, read_buf, BUFFER_SIZE - 1, 0);
            printf("%s\n", read_buf);
        }

        if(fds[0].revents & POLLIN){
            ret = splice(0, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
            ret = splice(pipefd[0], NULL, client_fd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE);
        }
    }

    close(client_fd);
    return 0;
}
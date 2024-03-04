#define _GNU_SOURCE 1
#include<sys/types.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include<stdlib.h>
#include<poll.h>
#include "../src/support.h"

#define USER_LIMIT 5
#define BUFFER_SIZE 64
#define FD_LIMIT 65535

struct client_data{
    sockaddr_storage address;
    char *write_buf;
    char buf[BUFFER_SIZE];
};

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    struct sockaddr_storage client_address;
    socklen_t client_len;
    int listen_fd, connfd, user_count = 0;
    client_data *users = new client_data[FD_LIMIT];
    pollfd fds[USER_LIMIT + 1];
    const char *ip = argv[1];
    const char *port = argv[2];

    if((listen_fd = open_listenfd(port)) < 0){
        fprintf(stderr, "can't open listenfd for %s\n", port);
        return 1;
    }

    for(int i = 1; i <= USER_LIMIT; ++i){
        fds[i].fd = -1;
        fds[i].events = 0;
    }
    fds[0].fd = listen_fd;
    fds[0].events = POLLIN | POLLERR;
    fds[0].revents = 0;

    while(true){
        int ret = poll(fds, user_count + 1, -1);
        if(ret < 0){
            fprintf(stderr, "poll error\n");
            break;
        }

        for(int i = 0; i <= user_count; ++i){
            if((fds[i].fd == listen_fd) && fds[i].revents & POLLIN){
                connfd = accept(listen_fd, (sockaddr * ) &client_address, &client_len);
                if(connfd < 0){
                    printf("receive connection fail, errno is: %s\n", strerror(errno));
                    continue;
                }
                if(user_count >= USER_LIMIT){
                    const char * info = "too many users\n";
                    printf("%s", info);
                    send(connfd, info, strlen(info), 0);
                    close(connfd);
                    continue;
                }

                ++user_count;
                users[connfd].address = client_address;
                setnonblocking(connfd);
                fds[user_count].fd = connfd;
                fds[user_count].events = POLLIN | POLLRDHUP | POLLERR;
                fds[user_count].revents = 0;
                printf("comes a new user, now have %d users\n", user_count);
            }else if(fds[i].revents & POLLERR){
                printf("get an error from connfd %d\n", fds[i].fd);
                char errors[100];
                socklen_t length = sizeof(errors);
                memset(errors, '\0', 100);
                if(getsockopt(fds[i].fd, SOL_SOCKET, SO_ERROR, errors, &length) < 0){
                    printf("getsockopt POLLERR failed\n");
                    continue;
                }
                printf("getsockopt POLLERR %s\n", errors);
                continue;
            }else if(fds[i].revents & POLLRDHUP){
                int user_fd = fds[i].fd;
                users[user_fd].write_buf = nullptr;
                memset(users[user_fd].buf, '\0', BUFFER_SIZE);

                fds[i] = fds[user_count];
                --i;
                --user_count;
                printf("client left, free connfd %d\n", user_fd);
            }else if(fds[i].revents & POLLIN){
                int connfd = fds[i].fd;
                memset(users[connfd].buf, '\0', BUFFER_SIZE);
                ret = recv(connfd, users[connfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, users[connfd].buf, connfd);
                if(ret < 0){
                    if((errno != EAGAIN) && (errno != EWOULDBLOCK)){
                        close(connfd);

                        users[connfd].write_buf = nullptr;
                        memset(users[connfd].buf, '\0', BUFFER_SIZE);

                        fds[i] = fds[user_count];
                        --i;
                        --user_count;
                    }
                }else if(ret == 0){

                }else{
                    for(int j = 1; j <= user_count; ++j){
                        if(fds[j].fd == connfd){
                            continue;
                        }
                        fds[j].events &= ~POLLIN;
                        fds[j].events |= POLLOUT;
                        users[fds[j].fd].write_buf = users[connfd].buf;
                    }
                }
            }else if(fds[i].revents & POLLOUT){
                int connfd = fds[i].fd;
                if(!users[connfd].write_buf){
                    continue;
                }
                ret = send(connfd, users[connfd].write_buf, strlen(users[connfd].write_buf), 0);
                users[connfd].write_buf = NULL;
                fds[i].events &= ~POLLOUT;
                fds[i].events |= POLLIN;
            }
        }
    }

    delete[] users;
    close(listen_fd);
    return 0;
}
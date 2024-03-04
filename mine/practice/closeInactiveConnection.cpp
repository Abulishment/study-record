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
#include "ascendSortedTimerList.h"

#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_list timer_list;
static int epollfd = 0;

void sig_handler(int sig){
    int saved_errno = errno;
    int msg = sig;
    send(pipefd[1], (char *) &msg, 1, 0);
    errno = saved_errno;
}

void addsig(int sig){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, nullptr) != -1);
}

void timer_handler(){
    timer_list.tick();
    alarm(TIMESLOT);
}

void cb_func(client_data * user_data){
    assert(user_data);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    close(user_data->sockfd);
    printf("close inactive fd %d\n", user_data->sockfd);
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    const char * ip = argv[1];
    const char * port = argv[2];
    int listenfd;
    epoll_event events[MAX_EVENT_NUMBER];
    bool stop_server = false, timeout = false;
    client_data * users = new client_data[FD_LIMIT];

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "open_listenfd fails\n");
        return 1;
    }

    assert((epollfd = epoll_create(5)) >= 0);
    addfd_et(epollfd, listenfd, true);

    assert(socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd) != -1);
    setnonblocking(pipefd[1]);
    addfd_et(epollfd, pipefd[0], true);
    addsig(SIGTERM);
    addsig(SIGINT);
    addsig(SIGALRM);
    alarm(TIMESLOT);

    while(!stop_server){
        int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
        if((number < 0) && (errno != EINTR)){
            printf("epoll failurel");
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;

            if(sockfd == listenfd){
                struct sockaddr_storage client_address;
                socklen_t clientlen;
                int clientfd;
                assert((clientfd = accept(listenfd, (sockaddr*) &client_address, &clientlen)) >= 0);
                addfd_et(epollfd, clientfd, true);
                users[clientfd].address = client_address;
                users[clientfd].sockfd = clientfd;
                users[clientfd].length = clientlen;

                util_timer *timer = new util_timer;
                timer->user_data = &users[clientfd];
                timer->cb_func = cb_func;
                time_t cur = time(nullptr);
                timer->absolute_expire = cur + 3 * TIMESLOT;
                users[clientfd].timer = timer;
                timer_list.add_timer(timer);
            }else if((sockfd == pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                int ret = recv(pipefd[0], signals, 1024, 0);
                if(ret == -1){
                    printf("read from pipefd[0] fails\n");
                    break;
                }else if(ret == 0){
                    printf("the other side closes the pipe\n");
                    continue;
                }else{
                    for(int j = 0; j < ret; ++j){
                        switch (signals[i])
                        {
                        case SIGALRM:
                            timeout = true;
                            break;
                        
                        case SIGTERM:
                        case SIGINT:
                            stop_server = true;
                            break;

                        default:
                            printf("ignore this signal %d\n", signals[i]);
                        }
                    }
                }
            }else if(events[i].events & EPOLLIN){
                memset(users[sockfd].buf, '\0', BUFFER_SIZE);
                int ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
                printf("get %d bytes of client data %s from %d\n", ret, users[sockfd].buf, sockfd);
                util_timer *timer = users[sockfd].timer;
                if(ret <= 0){
                    if(((errno != EAGAIN) && (errno != EWOULDBLOCK)) || (ret == 0)){
                        cb_func(&users[sockfd]);
                        timer_list.del_timer(timer);
                    }
                }else{
                    if(timer){
                        time_t cur = time(nullptr);
                        timer->absolute_expire = cur + TIMESLOT * 3;
                        printf("adjust timer once\n");
                        timer_list.adjust_timer(timer);
                    }
                }
            }else{
                printf("unexpected event\n");
            }

            if(timeout){
                printf("timeout once\n");
                timer_handler();
                timeout = false;
            }
        }
    }

    close(listenfd);
    close(pipefd[0]);
    close(pipefd[1]);
    close(epollfd);
    delete[] users;
    return 0;
}
#ifndef _process_poll_h_
#define _process_poll_h_

#include "../src/support.h"
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
#include<sys/epoll.h>
#include<signal.h>
#include<sys/wait.h>
#include<sys/stat.h>

struct Process{
    Process() : m_pid(-1){}
    pid_t m_pid;
    int m_pipefd[2];
};

template<typename T>
class ProcessPool{
private:
    ProcessPool(int listenfd, int process_number = 8);
public:
    static ProcessPool* create(int listenfd, int process_number = 8){
        if(!m_instance){
            m_instance = new ProcessPool(listenfd, process_number);
        }

        return m_instance;
    }

    virtual ~ProcessPool(){
        delete[] m_sub_process;
    }

    void run();
private:
    void setup_sig_pipe();
    void run_parent();
    void run_child();

    static const int MAX_PROCESS_NUMBER = 16;
    static const int USER_PER_PROCESS = 65536;
    static const int MAX_EVENT_NUMBER = 10000;
    int m_process_number;
    int m_idx;
    int m_epollfd;
    int m_listenfd;
    int m_stop;
    Process *m_sub_process;
    static ProcessPool *m_instance;
};

template<typename T>
ProcessPool<T>* ProcessPool<T>::m_instance = NULL;
static int sig_pipefd[2];

static void sig_handler(int sig){
    int saved_errno = errno;
    int msg = sig;
    send(sig_pipefd[1], (char*) &msg, 1, 0);
    errno = saved_errno;
}

static void addsig(int sig, void(*handler)(int), bool restart = true){
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

template<typename T>
ProcessPool<T>::ProcessPool(int listenfd, int process_number) : m_listenfd(listenfd), m_process_number(process_number), m_idx(-1), m_stop(false){
    assert((process_number > 0) && (process_number <= MAX_PROCESS_NUMBER));
    assert(this->m_sub_process = new Process[process_number]);

    for(int i = 0; i < process_number; ++i){
        int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_sub_process[i].m_pipefd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if(m_sub_process[i].m_pid > 0){
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }else{
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}

template<typename T>
void ProcessPool<T>::setup_sig_pipe(){
    assert((m_epollfd = epoll_create(5)) != -1);
    int ret = socketpair(PF_UNIX, SOCK_STREAM, 0, sig_pipefd);
    assert(ret != -1);
    setnonblocking(sig_pipefd[1]);
    addfd_et(m_epollfd, sig_pipefd[0],true);
    addsig(SIGCHLD, sig_handler);
    addsig(SIGTERM, sig_handler);
    addsig(SIGINT, sig_handler);
    addsig(SIGPIPE, SIG_IGN);
}

template<typename T>
void ProcessPool<T>::run(){
    if(m_idx != -1){
        run_child();
    }else{
        run_parent();
    }
}

template<typename T>
void ProcessPool<T>::run_child(){
    setup_sig_pipe();
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    addfd_et(m_epollfd, pipefd, true);

    T *users = new T[USER_PER_PROCESS];
    assert(users);
    epoll_event events[MAX_EVENT_NUMBER];
    int number = 0;
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);
        
        if((number < 0) && (number != EAGAIN) && (number != EWOULDBLOCK)){
            printf("epoll failure in %d\n", m_idx);
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;

            if((sockfd == pipefd) && (events[i].events & EPOLLIN)){
                int client = 0;
                ret = recv(sockfd, (char*) &client, sizeof(client), 0);
                if((ret < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)){
                    printf("recv from pipefd in %d fails\n", m_idx);
                    continue;
                }
                if(ret == 0){
                    printf("pipe close by main process in %d\n", m_idx);
                    close(pipefd);
                    continue;
                }
                struct sockaddr_storage client_address;
                socklen_t clientlen;
                int connfd = accept(m_listenfd, (sockaddr*) &client_address, &clientlen);
                if(connfd < 0){
                    printf("accept in childprocess %d fails : %s", m_idx, strerror(errno));
                    continue;
                }else{
                    addfd_et(m_epollfd, connfd, true);
                    users[connfd].init(m_epollfd, connfd, client_address, clientlen);
                }
            }else if((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0){
                    continue;
                }else{
                    for(int j = 0; j < ret; ++j){
                        switch (signals[j])
                        {
                        case SIGCHLD:
                            pid_t pid;
                            int stat;
                            while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                continue;
                            }
                            break;
                        case SIGTERM:
                        case SIGINT:
                            m_stop = true;
                            break; 
                        default:
                            break;
                        }
                    }
                }
            }else if(events[i].events & EPOLLIN){
                users[sockfd].process();
            }else{
                printf("unexpected event happens in child_process %d\n", m_idx);
            }
        }
    }

    delete[] users;
    users = NULL;
    close(pipefd);
    close(m_listenfd);
    close(m_epollfd);
}

template<typename T>
void ProcessPool<T>::run_parent(){
    setup_sig_pipe();
    addfd_et(m_epollfd, m_listenfd, true);
    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;

    while(!m_stop){
        number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);

        if((number < 0) && (number != EAGAIN) && (number != EWOULDBLOCK)){
            printf("epoll failure in %d\n", m_idx);
            break;
        }

        for(int i = 0; i < number; ++i){
            int sockfd = events[i].data.fd;

            if(sockfd == m_listenfd){
                int allocate = sub_process_counter;
                do{
                    if(m_sub_process[allocate].m_pid != -1){
                        break;
                    }
                    allocate = (allocate + 1) % m_process_number;
                }while(allocate != m_process_number);

                if(m_sub_process[allocate].m_pid == -1){
                    printf("There is no child_process remaining\n");
                    m_stop = true;
                    break;
                }

                sub_process_counter = (sub_process_counter + 1) % m_process_number;
                send(m_sub_process[allocate].m_pipefd[0], (char*) &new_conn, sizeof(new_conn), 0);
                printf("send a connection request to child_process %d\n", allocate);
            }else if((sockfd == sig_pipefd[0]) && (events[i].events & EPOLLIN)){
                int sig;
                char signals[1024];
                ret = recv(sig_pipefd[0], signals, sizeof(signals), 0);
                if(ret <= 0){
                    continue;
                }else{
                    for(int j = 0; j < ret; ++j){
                        switch (signals[j])
                        {
                        case SIGCHLD:
                            pid_t pid;
                            int stat;
                            while((pid = waitpid(-1, &stat, WNOHANG)) > 0){
                                for(int k = 0; k < m_process_number; ++k){
                                    if(m_sub_process[k].m_pid == pid){
                                        printf("child process %d aborts\n", k);
                                        close(m_sub_process[k].m_pipefd[0]);
                                        m_sub_process[k].m_pid = -1;
                                    }
                                }
                            }

                            m_stop = true;
                            for(int k = 0; k < m_process_number; ++k){
                                if(m_sub_process[k].m_pid != -1){
                                    m_stop = false;
                                }
                            }
                            break;
                        case SIGTERM:
                        case SIGINT:
                            printf("main_process receive term signal, kill all child_process"); 
                            for(int k = 0; k < m_process_number; ++k){
                                int pid = m_sub_process[k].m_pid;
                                if(pid != -1){
                                    kill(pid, SIGTERM);
                                }
                            }
                            break;
                        default:
                            break;
                        }
                    }
                }
            }else{
                printf("unexpected event happens in main_process\n");
            }
        }
    }

    close(m_listenfd);
    close(m_epollfd);
}

#endif

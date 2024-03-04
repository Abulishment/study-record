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

#include "../src/support.h"
#include "processPool.h"

class cgi_conn{
public:
    cgi_conn(){}
    ~cgi_conn(){}
    void init(int epollfd, int sockfd, const sockaddr_storage & client_address, socklen_t clientlen){
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = client_address;
        m_len = clientlen;
        memset(m_buf, '\0', BUFFER_SIZE);
        m_read_idx = 0;
    }

    void process(){
        int idx = 0;
        int ret = -1;
        while(true){
            idx = m_read_idx;
            ret = recv(m_sockfd, m_buf + idx, BUFFER_SIZE - 1 - idx, 0);
            if(((ret < 0) && (errno != EAGAIN) && (errno != EWOULDBLOCK)) || (ret == 0)){
                removefd(m_epollfd, m_sockfd);
            }else{
                m_read_idx += ret;
                printf("user content is : %s\n", m_buf);
                for(; idx < m_read_idx; ++idx){
                    if((idx >= 1) && (m_buf[idx - 1] == 'r') && (m_buf[idx] == '\n')){
                        break;
                    }
                }
                if(idx == m_read_idx){
                    continue;
                }
                m_buf[idx - 1] = '\0';
                char *file_name = m_buf;
                if(access(file_name, F_OK) == -1){
                    removefd(m_epollfd, m_sockfd);
                    break;
                }
                ret = fork();
                if(ret == -1){
                    removefd(m_epollfd, m_sockfd);
                    break;
                }else if(ret > 0){
                    removefd(m_epollfd, m_sockfd);
                    break;
                }else{
                    dup2(m_sockfd, STDOUT_FILENO);
                    execl(m_buf, m_buf, 0);
                    exit(0);
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;
    static int m_epollfd;
    int m_sockfd;
    sockaddr_storage m_address;
    socklen_t m_len;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;
};

int cgi_conn::m_epollfd = -1;

int main(int argc, char *argv[]){
    if(argc <= 2){
        printf("usage : %s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    int listenfd;
    const char * ip = argv[1];
    const char * port = argv[2];

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "open_listenfd fails\n");
        return 1;
    }

    ProcessPool<cgi_conn> * pool = ProcessPool<cgi_conn>::create(listenfd);
    if(pool){
        pool->run();
        delete pool;
    }

    close(listenfd);
    return 0;
}
#ifndef _web_server_h_
#define _web_server_h_

#include "../threadpool/threadPool.h"
#include "../http/httpConnection.h"
#include "../src/support.h"
#include "../log/logger.h"
#include "../timer/ascendSortedTimerList.h"
#include "../timer/timeWheelTimer.h"

#define MAX_EVENT_NUMBER 10000

class WebServer{
public:
    WebServer();
    virtual ~WebServer();
    void init(const char* port, bool set_reactor_mode = false);
    void eventloop();
private:
    void deal_connection();
    void deal_err(int sockfd);
    void deal_close(int sockfd);
    void deal_read(int sockfd);
    void deal_write(int sockfd);
    static void timer_cb_func(Http_Conn*);
    void timer_update(int sockfd);
    void init_signal();
    bool deal_signal(bool & timeout, bool & stop_server);
    static void signal_handler(int sig);
    ThreadPool<Http_Conn> *pool;
    time_wheel<Http_Conn> tw;
    Http_Conn* users;
    epoll_event events[MAX_EVENT_NUMBER];
    int user_count = 0;
    int epoll_fd = -1;
    int listen_fd = -1;
    /*0: reactor 1: proactor*/
    int actor_mode = 1;
};

#endif
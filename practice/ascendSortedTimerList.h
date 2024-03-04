#ifndef _ascend_sorted_timer_list_
#define _ascend_sorted_timer_list_
#define BUFFER_SIZE 64

#include<sys/socket.h>
#include <stdio.h>
#include <time.h>

#include "./http/httpConnection.h"

class util_timer;

struct client_data{
    struct sockaddr_storage address;
    socklen_t length;
    int sockfd;
    char buf[BUFFER_SIZE];
    util_timer *timer;
};

class util_timer{
public:
    util_timer() : prev(nullptr), next(nullptr){}
public:
    time_t absolute_expire;
    void(*cb_func)(client_data *);
    client_data *user_data;
    util_timer *next;
    util_timer *prev;
};

class sort_timer_list{
public:
    sort_timer_list() : head(nullptr), tail(nullptr){}
    virtual ~sort_timer_list(){
        while(head){
            util_timer *tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    void add_timer(util_timer * timer){
        if(timer == nullptr){
            return;
        }

        if(!head){
            head = tail = timer;
            return;
        }

        if(head->absolute_expire >= timer->absolute_expire){
            head->prev = timer;
            timer->next = head;
            timer->prev = nullptr;
            head = timer;
            return;
        }

        util_timer *first_above = head;
        while(first_above && first_above->absolute_expire < timer->absolute_expire){
            first_above = first_above->next;
        }

        if(first_above){
            util_timer *pre = first_above->prev;
            pre->next = timer;
            timer->next = first_above;
            first_above->prev = timer;
            timer->prev = pre;
        }else{
            tail->next = timer;
            timer->prev = tail;
            timer->next = nullptr;
            tail = timer;
        }
    }

    void del_timer(util_timer * timer){
        del_timer_sup(timer);
        free_timer(timer);
    }

    void adjust_timer(util_timer * timer){
        del_timer_sup(timer);
        add_timer(timer);
    }

    void tick(){
        if(!head){
            return;
        }

        printf("timer tick\n");
        time_t cur = time(nullptr);
        while(head){
            if(cur < head->absolute_expire){
                break;
            }

            head->cb_func(head->user_data);
            del_timer(head);
        }
    }

private:
    util_timer *head;
    util_timer *tail;
    void del_timer_sup(util_timer * timer){
        if(!timer){
            return;
        }

        if(timer == head){
            if(head == tail){
                head = tail = nullptr;
            }else{
                head = head->next;
                head->prev = nullptr;
            }
        }else if(timer == tail){
            tail = tail->prev;
            tail->next = nullptr;
        }else{
            util_timer *pre = timer->prev;
            util_timer *nex = timer->next;
            pre->next = nex;
            nex->prev = pre;
        }
    }

    void free_timer(util_timer * timer){
        delete timer;
    }
};

#endif
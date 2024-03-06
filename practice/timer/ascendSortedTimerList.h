#ifndef _ascend_sorted_timer_list_
#define _ascend_sorted_timer_list_
#define BUFFER_SIZE 64

#include<sys/socket.h>
#include <stdio.h>
#include <time.h>


template<typename T>
class util_timer{
public:
    util_timer<T>() : prev(nullptr), next(nullptr){}
public:
    time_t absolute_expire;
    void(*cb_func)(T *);
    T *user_data;
    util_timer<T> *next;
    util_timer<T> *prev;
};

template<typename T>
class sort_timer_list{
public:
    sort_timer_list() : head(nullptr), tail(nullptr){}
    virtual ~sort_timer_list(){
        while(head){
            util_timer<T> *tmp = head;
            head = head->next;
            delete tmp;
        }
    }

    void add_timer(util_timer<T> * timer){
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

        util_timer<T> *first_above = head;
        while(first_above && first_above->absolute_expire < timer->absolute_expire){
            first_above = first_above->next;
        }

        if(first_above){
            util_timer<T> *pre = first_above->prev;
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

    void del_timer(util_timer<T> * timer){
        del_timer_sup(timer);
        free_timer(timer);
    }

    void adjust_timer(util_timer<T> * timer){
        del_timer_sup(timer);
        add_timer(timer);
    }

    void tick(){
        if(!head){
            return;
        }

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
    util_timer<T> *head;
    util_timer<T> *tail;
    void del_timer_sup(util_timer<T> * timer){
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
            util_timer<T> *pre = timer->prev;
            util_timer<T> *nex = timer->next;
            pre->next = nex;
            nex->prev = pre;
        }
    }

    void free_timer(util_timer<T> * timer){
        delete timer;
    }
};

#endif
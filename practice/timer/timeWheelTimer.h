#ifndef _time_wheel_timer_
#define _time_wheel_timer_

#include<time.h>
#include<netinet/in.h>
#include<stdio.h>
#include "../lock/locker.h"

#define BUFFER_SIZE 64

template<typename T>
class tw_timer{
public:
    tw_timer<T>(int rot, int ts) : next(NULL), prev(NULL), rotation(rot), time_slot(ts) {} 
public:
    int rotation;
    int time_slot;
    void (*cb_func)(T *);
    T * user_data;
    tw_timer<T> * prev;
    tw_timer<T> * next;
};

template<typename T>
class time_wheel{
public:
    time_wheel() : cur_slot(0){
        for(int i = 0; i < N; ++i){
            slots[i] = nullptr;
        }
    }

    virtual ~time_wheel(){
        for(int i = 0; i < N; ++i){
            tw_timer<T> * tmp = slots[i];
            while(tmp){
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    tw_timer<T> * add_timer(int timeout){
        if(timeout < 0){
            return NULL;
        }

        int ticks = 0;

        if(timeout < SI){
            ticks = 1;
        }else{
            ticks = timeout / SI;
        }

        int rotation = ticks / N;
        int ts = (cur_slot + (ticks % N)) % N;
        tw_timer<T> * timer = new tw_timer<T>(rotation, ts);

        lock.lock();
        if(!slots[ts]){
            slots[ts] = timer;
        }else{
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        lock.unlock();
        // printf("add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot);

        return timer;
    }

    void del_timer(tw_timer<T> * timer){
        lock.lock();
        del_timer_sup(timer);
        delete timer;
        lock.unlock();
    }

    void adjust_timer(tw_timer<T> * timer, int timeout){
        if(timeout < 0){
            return;
        }

        int ticks = 0;

        if(timeout < SI){
            ticks = 1;
        }else{
            ticks = timeout / SI;
        }
        int rotation = ticks / N;
        int ts = (timer->time_slot + (ticks % N)) % N;

        lock.lock();
        del_timer_sup(timer);
        /*调整再加入时需要设置前后指针为NULL。s*/
        timer->prev = NULL;
        timer->next = NULL;

        if(!slots[ts]){
            slots[ts] = timer;
        }else{
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }

        timer->rotation += rotation;
        timer->time_slot = ts;
        lock.unlock();
    }

    void tick(){
        lock.lock();
        tw_timer<T> * tmp = slots[cur_slot];
        // printf("current slot is %d\n", cur_slot);

        while(tmp){
            // printf("tick the timer once\n");

            tw_timer<T> * store = tmp->next;
            if(tmp->rotation > 0){
                --(tmp->rotation);
            }else{
                tmp->cb_func(tmp->user_data);
                // if(tmp == slots[cur_slot]){
                //     printf("executed timer is head, delete\n");
                // }else{
                //     printf("executed timer is not head, delete\n");
                // }
                del_timer_sup(tmp);
                delete tmp;
            }

            tmp = store;
        }

        cur_slot = (++cur_slot) % N;
        lock.unlock();
    }
private:
    const static int N = 60;
    const static int SI = 1;
    tw_timer<T> *slots[N];
    int cur_slot;
    locker lock;

    void del_timer_sup(tw_timer<T> * timer){
        if(!timer){
            return;
        }

        int ts = timer->time_slot;
        if(timer == slots[ts]){
            slots[ts] = slots[ts]->next;
            if(slots[ts]){
                slots[ts]->prev = nullptr;
            }
        }else{
            timer->prev->next = timer->next;
            if(timer->next){
                timer->next->prev = timer->prev;
            }
        }
    }
};

#endif
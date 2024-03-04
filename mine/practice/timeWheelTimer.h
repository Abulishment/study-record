#ifndef _time_wheel_timer_
#define _timer_wheel_timer_

#include<time.h>
#include<netinet/in.h>
#include<stdio.h>

#define BUFFER_SIZE 64

class tw_timer;

struct client_data{
    sockaddr_storage address;
    socklen_t length;
    int sockfd;
    tw_timer *timer;
    char buf[BUFFER_SIZE];
};

class tw_timer{
public:
    tw_timer(int rot, int ts) : next(NULL), prev(NULL), rotation(rot), time_slot(ts) {} 
public:
    int rotation;
    int time_slot;
    void (*cb_func)(client_data *);
    client_data * user_data;
    tw_timer * prev;
    tw_timer * next;
};

class time_wheel{
public:
    time_wheel() : cur_slot(0){
        for(int i = 0; i < N; ++i){
            slots[i] = nullptr;
        }
    }

    virtual ~time_wheel(){
        for(int i = 0; i < N; ++i){
            tw_timer * tmp = slots[i];
            while(tmp){
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    tw_timer * add_timer(int timeout){
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
        tw_timer * timer = new tw_timer(rotation, ts);

        if(!slots[ts]){
            slots[ts] = timer;
        }else{
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        printf("add timer, rotation is %d, ts is %d, cur_slot is %d\n", rotation, ts, cur_slot);

        return timer;
    }

    void del_timer(tw_timer * timer){
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
        delete timer;
    }

    void tick(){
        tw_timer * tmp = slots[cur_slot];
        printf("current slot is %d\n", cur_slot);

        while(tmp){
            printf("tick the timer once\n");

            tw_timer * store = tmp->next;
            if(tmp->rotation > 0){
                --tmp->rotation;
            }else{
                tmp->cb_func(tmp->user_data);
                if(tmp == slots[cur_slot]){
                    printf("executed timer is head, delete\n");
                }else{
                    printf("executed timer is not head, delete\n");
                }
                del_timer(tmp);
            }

            tmp = store;
        }

        cur_slot = (++cur_slot) % N;
    }
private:
    const static int N = 60;
    const static int SI = 1;
    tw_timer *slots[N];
    int cur_slot;
};

#endif
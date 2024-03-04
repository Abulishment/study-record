#ifndef _min_heap_timer_
#define _min_heap_timer_

#include<iostream>
#include<netinet/in.h>
#include<time.h>

#define BUFFER_SIZE 64

using std::exception;

class heap_timer;

struct client_data{
    sockaddr_storage address;
    socklen_t length;
    int sockfd;
    char buf[BUFFER_SIZE];
    heap_timer * timer;
};

struct heap_timer{
    heap_timer(int delay){
        this->absolute_expire = time(nullptr) + delay;
    }
    time_t absolute_expire;
    void(*cb_func)(client_data *);
    client_data * user_data;
};

class time_heap{
public:
    time_heap(int cap) noexcept : capacity(cap), cur_size(0){
        array = new heap_timer*[capacity];
        if(!array){
            throw std::exception();
        }

        for(int i = 0; i < capacity; ++i){
            array[i] = nullptr;
        }
    }

    time_heap(heap_timer ** init_array, int size, int capacity) noexcept : cur_size(size), capacity(capacity){
        if(capacity < size){
            throw std::exception();
        }
        array = new heap_timer *[capacity];
        if(!array){
            throw std::exception();
        }

        for(int i = 0; i < capacity; ++i){
            array[i] = nullptr;
        }

        if(size != 0){
            for(int i = 0; i < size; ++i){
                array[i] = init_array[i];
            }
            for(int i = (cur_size - 1) / 2; i >= 0; --i){
                percolate_down(i);
            }
        }
    }

    virtual ~time_heap(){
        for(int i = 0; i < cur_size; ++i){
            delete array[i];
        }
        delete[] array;
    }

    void add_timer(heap_timer * timer){
        if(!timer){
            return;
        }

        if(cur_size >= capacity){
            resize();
        }

        int hole = cur_size++;
        int parent = 0;

        for(; hole > 0; hole = parent){
            parent = (hole - 1) / 2;
            if(array[parent]->absolute_expire <= timer->absolute_expire){
                break;
            }
            array[hole] = array[parent];
        }

        array[hole] = timer;
    }

    void del_timer(heap_timer * timer){
        if(!timer){
            return;
        }
        timer->cb_func = nullptr;
    }

    heap_timer *top() const{
        if(empty()){
            return nullptr;
        }
        return array[0];
    }

    void pop_timer(){
        if(empty()){
            return ;
        }

        if(array[0]){
            delete array[0];
            array[0] = array[--cur_size];
            percolate_down(0);
        }
    }

    void tick(){
        heap_timer * tmp = array[0];
        time_t cur = time(nullptr);

        while(!empty()){
            if(!tmp){
                break;
            }

            if(tmp->absolute_expire > cur){
                break;
            }

            if(array[0]->cb_func){
                array[0]->cb_func(array[0]->user_data);
            }

            pop_timer();
            tmp = array[0];
        }
    }

    bool empty() const{return cur_size == 0;}
private:
    heap_timer **array;
    int capacity;
    int cur_size;

    void percolate_down(int hole){
        int child = 0;
        for(; (hole * 2 + 1) <= (cur_size - 1); hole = child){
            child = hole * 2 + 1;
            if((child < cur_size - 1) && (array[child + 1]->absolute_expire < array[child]->absolute_expire)){
                ++child;
            }
            if(array[child]->absolute_expire < array[hole]->absolute_expire){
                heap_timer * tmp = array[hole];
                array[hole] = array[child];
                array[child] = tmp;
            }else{
                break;
            }
        }
    }

    void resize(){
        heap_timer **tmp = new heap_timer*[2 * capacity];
        for(int i = 0; i < 2 * capacity; ++i){
            tmp[i] = nullptr;
        }

        capacity = 2 * capacity;
        for(int i = 0; i < cur_size; ++i){
            tmp[i] = array[i];
        }

        delete[] array;
        array = tmp;
    }
};

#endif
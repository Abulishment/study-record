#ifndef _BlockQueue_h_
#define _BlockQueue_h_

#include<semaphore.h>
#include<stdio.h>
#include<utility>
#include<errno.h>
#include<string.h>
#include<stdlib.h>

template<typename T>
class BlockQueue{
public:
    BlockQueue(int size){
        max_size = size;
        buf = new T[max_size];
    }
    virtual ~BlockQueue(){
        delete[] buf;
    }

    bool empty(){
        int sem_value;
        sem_getvalue(&slots, &sem_value) == max_size;
        return sem_value == max_size;
    }

    void init(){
        front = 0;
        rear = 0;
        sem_init(&mutex, 0, 1);
        sem_init(&slots, 0, max_size);
        sem_init(&items, 0, 0);
    }

    void insert(T && item){
        sem_wait(&slots);
        sem_wait(&mutex);
        buf[(++(rear)) % max_size] = item;
        sem_post(&mutex);
        sem_post(&items);
    }

    void remove(T & item){
        sem_wait(&items);
        sem_wait(&mutex);
        item = std::move(buf[++(front) % max_size]);
        sem_post(&mutex);
        sem_post(&slots);
    }
private:
    T *buf;
    int max_size;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
};

#endif
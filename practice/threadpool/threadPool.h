#ifndef _thread_pool_h
#define _thread_pool_h

#include<list>
#include<cstdio>
#include<exception>
#include<pthread.h>
#include "../lock/locker.h"
#include "../log/logger.h"

template<typename T>
class ThreadPool{
public:
    ThreadPool(int thread_number = 8, int max_requests = 10000);
    virtual ~ThreadPool();
    bool append(T* request);
private:
    static void* worker(void *arg);
    void run();
    int m_thread_number;
    int m_max_requests;
    pthread_t *m_threads;
    std::list<T*> m_workqueue;
    locker m_queuelocker;
    sem m_queuestat;
    bool m_stop;
};

template<typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests): m_thread_number(thread_number), m_max_requests(max_requests), m_stop(false), m_threads(nullptr){
    if((thread_number <= 0) || (max_requests <= 0)){
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw std::exception();
    }

    for(int i = 0; i < m_thread_number; ++i){
        // printf("create the %dth thread\n", i);
        LOG(std::string("create the ") + std::to_string(i) + "th thread\n");
        if(pthread_create(m_threads + i, NULL, worker, this) != 0){
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool(){
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool ThreadPool<T>::append(T *request){
    m_queuelocker.lock();
    if(m_workqueue.size() > m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<typename T>
void *ThreadPool<T>::worker(void *arg){
    ThreadPool *pool = (ThreadPool*) arg;
    pool->run();
    return pool;
}

template<typename T>
void ThreadPool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request){
            continue;
        }
        request->process();
    }
}

#endif
#ifndef _locker_h_
#define _locker_h_

#include<exception>
#include<pthread.h>
#include<semaphore.h>

class sem{
public:
    sem();
    virtual ~sem();
    bool wait();
    bool post();
private:
    sem_t m_sem;
};

class locker{
public:
    locker();
    virtual ~locker();
    bool lock();
    bool unlock();
private:
    pthread_mutex_t m_sem;
};

class cond{
public:
    cond();
    virtual ~cond();
    bool wait();
    bool signal();
private:
    pthread_mutex_t m_mutex;
    pthread_cond_t m_cond;
};

#endif
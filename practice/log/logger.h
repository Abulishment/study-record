#ifndef _logger_h_
#define _logger_h_

#include<string>
#include<fstream>
#include "blockQueue.h"
#include "../lock/locker.h"

class Logger{
public:
    static Logger* getInstance(){
        static Logger instance;
        return &instance;
    }
    
    void init(const char * filename, int write_thread_count = 3, int block_queue_size = 300){
        if(block_queue){
            delete block_queue;
        }
        block_queue = new BlockQueue<std::string>(block_queue_size);
        block_queue->init();
        ofs.open(filename);
        stop_logger = false;
        this->write_thread_count = write_thread_count;
        create_worker_thread();
    }

    void push(std::string && str){
        block_queue->insert(std::move(str));
    }

    bool is_stop(){
        return stop_logger;
    }

    void stop(){
        stop_logger = false;
    }

private:
    Logger(){
    }
    virtual ~Logger(){}

    static void* worker(void * arg){
        Logger *logger = (Logger*) arg;
        pthread_detach(pthread_self());
        while(!logger->is_stop() || !logger->block_queue->empty()){
            logger->write_log();
        }
        return NULL;
    }

    void create_worker_thread(){
        pthread_t pid;
        for(int i = 0; i < write_thread_count; ++i){
            pthread_create(&pid, NULL, worker, getInstance());
        }
    }

    void write_log(){
        std::string info;
        block_queue->remove(info);
        lock.lock();
        ofs << std::move(info);
        lock.unlock();
    }

    std::ofstream ofs; 
    locker lock;
    int write_thread_count;
    BlockQueue<std::string> *block_queue = NULL;
    bool stop_logger;
};

#define LOG(info) Logger::getInstance()->push(info) 
#endif
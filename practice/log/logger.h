#ifndef _logger_h_
#define _logger_h_

#include<string>
#include<fstream>
#include "blockQueue.h"
#include "../lock/locker.h"
#include <time.h>
#include <ctime>

class Logger{
public:
    static Logger* getInstance();   
    static void addItem(std::string && item);
    void init(const char * filename, bool set_async_mode = false, bool stop_logger = false, int write_thread_count = 3, int block_queue_size = 300);
    void push(std::string && str);
    bool is_stop();
    void stop();
private:
    Logger();
    virtual ~Logger();
    static void* worker(void * arg);
    void create_worker_thread();
    void write_log();
    static std::string get_time();

    std::ofstream ofs; 
    locker lock;
    int write_thread_count;
    BlockQueue<std::string> *block_queue = NULL;
    bool stop_logger = false;
    bool async_mode = false;
};

#define LOG(info) Logger::addItem(info) 

#endif
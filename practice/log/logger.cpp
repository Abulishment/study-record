#include "logger.h"

Logger* Logger::getInstance(){
    static Logger instance;
    return &instance;
}

void Logger::init(const char * filename, bool set_async_mode, bool stop_logger, int write_thread_count, int block_queue_size){
    this->stop_logger = stop_logger;
    this->async_mode = set_async_mode;

    if(stop_logger){
        return;
    }

    if(async_mode){
        block_queue = new BlockQueue<std::string>(block_queue_size);
        block_queue->init();
        this->write_thread_count = write_thread_count;
        create_worker_thread();
    }else{
        this->write_thread_count = 0;
    }
    ofs.open(filename);
}

void Logger::push(std::string && str){
    block_queue->insert(get_time() + " " + std::move(str));
}

bool Logger::is_stop(){
    return stop_logger;
}

void Logger::stop(){
    stop_logger = true;
}

Logger::Logger(){}
Logger::~Logger(){
    if(!block_queue){
        delete block_queue;
    }
}

void* Logger::worker(void * arg){
    Logger *logger = (Logger*) arg;
    pthread_detach(pthread_self());
    while(!logger->is_stop() || !logger->block_queue->empty()){
        logger->write_log();
    }
    return NULL;
}

void Logger::create_worker_thread(){
    pthread_t pid;
    for(int i = 0; i < write_thread_count; ++i){
        pthread_create(&pid, NULL, worker, getInstance());
    }
}

void Logger::write_log(){
    std::string info;
    block_queue->remove(info);
    lock.lock();
    ofs << std::move(info);
    lock.unlock();
}

std::string Logger::get_time(){
    std::time_t currentTime = std::time(nullptr);
    const int bufferSize = 80; 
    char buffer[bufferSize];
    buffer[0] = '[';
    std::strftime(buffer + 1, bufferSize - 1, "%Y-%m-%d %H:%M:%S", std::localtime(&currentTime));   
    return std::string(buffer) + "]";
}

void Logger::addItem(std::string && item){
    if(Logger::getInstance()->is_stop()){
        return;
    }

    if(Logger::getInstance()->async_mode){
        Logger::getInstance()->push(std::move(item));
    }else{
        Logger::getInstance()->lock.lock();
        Logger::getInstance()->ofs << get_time() + " " + std::move(item);
        Logger::getInstance()->lock.unlock();
    }
}
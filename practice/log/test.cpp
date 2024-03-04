#include "blockQueue.h"
#include "logger.h"
#include<string>

using namespace std;

int main(){
    Logger::getInstance()->init("./log.txt");
    while(true){
        LOG("hello ");
    }
    return 0;
}
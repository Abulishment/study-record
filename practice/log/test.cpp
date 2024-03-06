#include "blockQueue.h"
#include "logger.h"
#include<string>

using namespace std;

int main(){
    Logger::getInstance()->init("./log.txt", true, false);
    while(true){
        static int i = 0;
        LOG(to_string(i++) + '\n');
    }
    return 0;
}
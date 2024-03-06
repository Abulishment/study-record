#include <stdio.h>
#include "log/logger.h"
#include "./web_server/webServer.h"


int main(int argc, char *argv[]){
    if(argc <= 1){
        fprintf(stderr, "usage : %s [port_number]\n", basename(argv[0]));
        return 1;
    }
    const char * port = argv[1];
    Logger::getInstance()->init("./log.txt", false, false);
    WebServer ws;
    ws.init(port, true);
    ws.eventloop();
    return 0;
}
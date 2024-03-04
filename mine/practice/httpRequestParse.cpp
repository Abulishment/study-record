#include<sys/socket.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<assert.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<errno.h>
#include<string.h>
#include<fcntl.h>
#include "../src/support.h"

#define BUFFER_SIZE 4096

enum CHECK_STATE{CHECK_STATE_REQUESTLINE = 0, CHECK_STATE_HEADER};
enum LINE_STATUS{LINE_OK = 0, LINE_BAD, LINE_OPEN};
enum HTTP_CODE{NO_REQUEST, GET_REQUEST, BAD_REQUEST, FORBIDDEN_REQUEST, INTERNAL_ERROR, CLOSED_CONNECTION};

static const char *szret[] = {"I get a correct result\n", "Something wrong\n"};
static const int szret_len0 = strlen(szret[0]), szret_len1 = strlen(szret[1]);

LINE_STATUS parse_line(char *buffer, int & checked_index, int & read_index){
    char temp;
    for(; checked_index < read_index; ++checked_index){
        temp = buffer[checked_index];
        if(temp == '\r'){
            if(checked_index + 1 == read_index){
                return LINE_OPEN;
            }else if(buffer[checked_index + 1] == '\n'){
                buffer[checked_index++] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp == '\n'){
            if((checked_index > 1) && buffer[checked_index - 1] == '\r'){
                buffer[checked_index - 1] = '\0';
                buffer[checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    return LINE_OPEN;
}

HTTP_CODE parse_requestline(char * temp, CHECK_STATE & checkstate){
    char *uri = strpbrk(temp, " \t");
    if(!uri){
        return BAD_REQUEST;
    }
    *uri++ = '\0';

    char *method = temp;
    if(strcasecmp(method, "GET") == 0){
        printf("The request method is: GET\n");
    }else{
        return BAD_REQUEST;
    }

    uri += strspn(uri, " \t");

    char *version = strpbrk(uri, " \t");
    if(!version){
        return BAD_REQUEST;
    }
    *version++ = '\0';
    version += strspn(version, " \t");
    if(strcasecmp(version, "HTTP/1.1") != 0){
        return BAD_REQUEST;
    }

    if(strncasecmp(uri, "http://", 7) == 0){
        uri += 7;
        uri = strchr(uri, '/');
    }
    if(!uri || uri[0] != '/'){
        return BAD_REQUEST;
    }

    printf("The request URI is: %s\n", uri);
    checkstate = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

HTTP_CODE parse_headers(char *temp){
    if(temp[0] == '\0'){
        return GET_REQUEST;
    }else if(strncasecmp(temp, "Host:", 5) == 0){
        temp += 5;
        temp += strspn(temp, " \t");
        printf("the request host is: %s\n", temp);
    }else{
        printf("I can not handle this header\n");
    }

    return NO_REQUEST;
}

HTTP_CODE parse_content(char *buffer, int & checked_index, CHECK_STATE & check_state, int & read_index, int & start_line){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE retcode = NO_REQUEST;
    while((line_status = parse_line(buffer, checked_index, read_index)) == LINE_OK){
        char *temp = buffer + start_line;
        start_line = checked_index;
        switch (check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            retcode = parse_requestline(temp, check_state);
            if(retcode == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            retcode = parse_headers(temp);
            if(retcode == BAD_REQUEST){
                return BAD_REQUEST;
            }else if(retcode == GET_REQUEST){
                return GET_REQUEST;
            }
            break;
        default:
            return INTERNAL_ERROR;
            break;
        }
    }

    if(line_status == LINE_OPEN){
        return NO_REQUEST;
    }else{
        return BAD_REQUEST;
    }
}

int main(int argc, char *argv[]){
    if(argc <= 2){
        fprintf(stderr, "usage:%s [ip_address] [port_number]\n", basename(argv[0]));
        return 1;
    }

    int listenfd, connfd, data_read = 0, read_index = 0, checked_index = 0, start_line = 0;
    CHECK_STATE check_state = CHECK_STATE_REQUESTLINE;
    HTTP_CODE result;
    struct sockaddr_storage clientaddr;
    socklen_t clientlen;
    char buffer[BUFFER_SIZE];
    const char *ip = argv[1];
    const char *port = argv[2];

    if((listenfd = open_listenfd(port)) < 0){
        fprintf(stderr, "can't open listenfd %s\n", port);
        return 1;
    }

    if((connfd = accept(listenfd, (sockaddr *) &clientaddr, &clientlen)) < 0){
        fprintf(stderr, "accept error: %s\n", strerror(errno)); 
        return -1;
    }else{
        memset(buffer, '\0', BUFFER_SIZE); 
        while(true){
            if((data_read = recv(connfd, buffer + read_index, BUFFER_SIZE - read_index, 0)) == -1){
                fprintf(stderr, "reading failed: %s\n", strerror(errno));
                break;
            }else if(data_read == 0){
                fprintf(stderr, "remote client has closed the connection\n");
                break;
            }

            read_index += data_read;
            result = parse_content(buffer, checked_index, check_state, read_index, start_line);
            if(result == NO_REQUEST){
                continue;
            }else if(result == GET_REQUEST){
                send(connfd, szret[0], szret_len0, 0);
                break;
            }else{
                send(connfd, szret[1], szret_len1, 0);
                break;
            }
        }
        close(connfd);
    }

    close(listenfd);
    return 0;
}
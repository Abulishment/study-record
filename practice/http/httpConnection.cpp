#include "httpConnection.h"
#include "../src/support.h"
#include "../../../../../usr/include/x86_64-linux-gnu/sys/uio.h"

const char * ok_200_title = "OK";
const char * error_400_title = "Bad Request";
const char * error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const char * error_403_title = "Forbidden";
const char * error_403_form = "You do not have permission to get file from this server.\n";
const char * error_404_title = "Not Found";
const char * error_404_form = "Thre requested file was not found on this server.\n";
const char * error_500_title = "Internal Error";
const char * error_500_form = "There was an unusual problem serving the requested file.\n";
const char * doc_root = ".";

int Http_Conn::m_user_count = 0;
int Http_Conn::m_epollfd = -1;
static bool print = true;

void Http_Conn::close_conn(bool real_close){
    if(real_close && (m_sockfd != -1)){
        if(print)
        printf("close %d(fd)\n", m_sockfd);
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

void Http_Conn::init(int sockfd, const sockaddr_storage & addr, socklen_t addrlen){
    m_sockfd = sockfd;
    m_address = addr;
    m_address_len = addrlen;

    //debug : avoid TIME_WAIT state.
    int reuse = 1;
    setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    addfd(m_epollfd, sockfd, true, true);
    m_user_count++;
    init();
}

void Http_Conn::init(){
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    bytes_to_send = 0;
    bytes_have_send = 0;
    memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
}

Http_Conn::LINE_STATUS Http_Conn::parse_line(){
    char temp;
    for(; m_checked_idx < m_read_idx; ++m_checked_idx){
        temp = m_read_buf[m_checked_idx];
        if(temp == '\r'){
            if((m_checked_idx + 1 == m_read_idx)){
                return LINE_OPEN;
            }else if(m_read_buf[m_checked_idx + 1] == '\n'){
                m_read_buf[m_checked_idx++] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }else if(temp == '\n'){
            if((m_checked_idx > 1) && (m_read_buf[m_checked_idx - 1] == '\r')){
                m_read_buf[m_checked_idx - 1] = '\0';
                m_read_buf[m_checked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }

    return LINE_OPEN;
}

bool Http_Conn::read(){
    if(m_read_idx >= READ_BUFFER_SIZE){
        return false;
    }

    int bytes_read = 0;
    while(true){
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1){
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                if(print)
                printf("read success in %d\n", m_sockfd);
                break;
            }
            //error
            printf("read error : %s\n", strerror(errno));
            return false;
        }else if(bytes_read == 0){
            //close by foreignal host;
            printf("connection closed by foreignal host %d\n", m_sockfd);
            return false;
        }
        m_read_idx += bytes_read;
    }

    return true;
}

Http_Conn::HTTP_CODE Http_Conn::parse_request_line(char *text){
    m_url = strpbrk(text, " \t");
    if(!m_url){
        return BAD_REQUEST;
    }

    *m_url++ = '\0';
    char *method = text;
    if(strcasecmp(method, "GET") == 0){
        m_method = GET;
    }else{
        return BAD_REQUEST;
    }

    m_url += strspn(m_url, " \t");

    m_version = strpbrk(m_url, " \t");
    if(!m_version){
        return BAD_REQUEST;
    }
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if(strcasecmp(m_version, "HTTP/1.1") != 0 && strcasecmp(m_version, "HTTP/1.0") != 0){
        return BAD_REQUEST;
    }

    if(strncasecmp(m_url, "http://", 7) == 0){
        m_url += 7;
        m_url = strchr(m_url, '/');
    }
    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }

    if(!m_url || m_url[0] != '/'){
        return BAD_REQUEST;
    }

    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

Http_Conn::HTTP_CODE Http_Conn::parse_headers(char * text){
    if(text[0] == '\0'){
        //http请求还有m_content_length字节的消息体要读。
        if(m_content_length != 0){
            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }else if(strncasecmp(text, "Connection:", 11) == 0){
        text += 11;
        text += strspn(text, " \t");
        if(strcasecmp(text, "keep-alive") == 0){
            m_linger = true;
        }
    }else if(strncasecmp(text, "Content-Length:", 15) == 0){
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }else if(strncasecmp(text, "Host:", 5) == 0){
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }else{
        if(print)
        printf("oops!unknown header%s\n", text);
    }

    return NO_REQUEST;
}

/*不检查消息体，仅判断是否完整读入该消息体。*/
Http_Conn::HTTP_CODE Http_Conn::parse_content(char * text){
    if(m_read_idx >= (m_content_length + m_checked_idx)){
        text[m_content_length] = '\0';
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

Http_Conn::HTTP_CODE Http_Conn::process_read(){
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char * text = 0;
    while(((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK)) || ((line_status = parse_line()) == LINE_OK)){
        text = get_line();
        m_start_line = m_checked_idx;
        if(print)
        printf("got 1 http line : %s in %d\n", text, m_sockfd);

        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }
            break;
        case CHECK_STATE_HEADER:
            ret = parse_headers(text); 
            if(ret == BAD_REQUEST){
                return BAD_REQUEST;
            }else if(ret == GET_REQUEST){
                return do_request();
            }
            break;
        case CHECK_STATE_CONTENT:
            ret = parse_content(text);
            if(ret == GET_REQUEST){
                return do_request();
            }
            line_status = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

//得到一个完整http请求时，回应该请求。分析文件属性，其是否合法（存在，可读，且非目录）。若合法，映射至内存地址m_file_address处，并通知调用者获取文件成功
Http_Conn::HTTP_CODE Http_Conn::do_request(){
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    if(stat(m_real_file, &m_file_stat) < 0){
        return NO_RESOURCE;
    }

    if(!(m_file_stat.st_mode & S_IROTH)){
        return FORBIDDEN_REQUEST;
    }
    if(S_ISDIR(m_file_stat.st_mode)){
        return BAD_REQUEST;
    }

    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char*) mmap(NULL, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

void Http_Conn::unmap(){
    if(m_file_address){
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool Http_Conn::write(){
    int temp = 0;
    if(bytes_to_send == 0){
        modfd(m_epollfd, m_sockfd, EPOLLIN, true);
        init();
        return true;
    }

    while(true){
        temp = writev(m_sockfd, m_iv, m_iv_count);
        // writev(STDOUT_FILENO, m_iv, m_iv_count);
        if(temp < 0){
            if((errno == EAGAIN) || (errno == EWOULDBLOCK)){
                printf("write in %d blocks, try next time\n", m_sockfd);
                modfd(m_epollfd, m_sockfd, EPOLLOUT, true);
                return true;
            }
            unmap();
            return false;
        }

        bytes_to_send -= temp;
        bytes_have_send += temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }
        /*TAG*/
        if(bytes_to_send <= 0){
            /*send successfully*/
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN, true);
            if(m_linger){
                if(print)
                printf("write success in %d, keep-alive\n", m_sockfd);
                if(m_checked_idx < m_read_idx){
                    if(print)
                    printf("next request has come and will be discard %d : %s\n", m_sockfd, &m_read_buf[m_checked_idx]);
                }
                init();
                return true;
            }else{
                if(print)
                printf("write success in %d, close\n", m_sockfd);
                return false;
            }
        }
    }
}

bool Http_Conn::add_response(const char * format, ...){
    if(m_write_idx >= WRITE_BUFFER_SIZE){
        return false;
    }
    /*variadic arguments list*/
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - m_write_idx - 1, format, arg_list);
    if(len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx)){
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    return true;
}

bool Http_Conn::add_status_line(int status, const char * title){
    return add_response("%s %d %s\r\n", "HTTP/1.0", status, title);
}

bool Http_Conn::add_headers(int content_len){
    return add_content_length(content_len) && add_linger() && add_blank_line();
}

bool Http_Conn::add_content_length(int content_len){
    return add_response("Content-Length:%d\r\n", content_len);
}

bool Http_Conn::add_linger(){
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

bool Http_Conn::add_blank_line(){
    return add_response("%s", "\r\n");
}

bool Http_Conn::add_content(const char * content){
    return add_response("%s", content);
}

bool Http_Conn::process_write(HTTP_CODE ret){
    switch (ret)
    {
    case INTERNAL_ERROR:
        if(print)
            printf("invalid response\n");
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form));
        if(!add_content(error_500_form)){
            return false;
        }
        break;
    case NO_RESOURCE:
        if(print)
            printf("invalid response\n");
        add_status_line(404, error_404_title); 
        add_headers(strlen(error_404_form));
        if(!add_content(error_404_form)){
            return false;
        }
        break;
    case FORBIDDEN_REQUEST:
        if(print)
            printf("invalid response\n");
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if(!add_content(error_403_form)){
            return false;
        }
        break;
    case FILE_REQUEST:
        if(print)
            printf("valid response\n");
        add_status_line(200, ok_200_title);
        if(m_file_stat.st_size != 0){
            add_headers(m_file_stat.st_size);
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }else{
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string));
            if(!add_content(ok_string)){
                return false;
            }
        }
        break;
    default:
        return false;
        break;
    }

    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}

void Http_Conn::process(){
    if(print)
    printf("start process %d\n", m_sockfd);
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST){
        modfd(m_epollfd, m_sockfd, EPOLLIN, true);
        return;
    }
    bool write_ret = process_write(read_ret);
    //对于长连接可能有丢失请求的情况发生。
    if(!write_ret){
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT, true);
}
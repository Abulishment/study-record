#ifndef _support_h_
#define _support_h_

int open_listenfd(const char *port, bool isTcp = true);
int open_clientfd(const char *hostname, const char *port, bool isTcp = true);
int setnonblocking(int fd);
void removefd(int epollfd, int fd);
void addsig(int sig, void(*handler)(int), bool restart = true);
void send_error(int connfd, const char * info);
void addfd(int epollfd, int fd, bool enable_et, bool enable_oneshot);
void modfd(int epollfd, int fd, int ev, bool enable_et);
#endif
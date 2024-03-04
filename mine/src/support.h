#ifndef _support_h_
#define _support_h_

int open_listenfd(const char *port, bool isTcp = true);
int open_clientfd(const char *hostname, const char *port, bool isTcp = true);
int setnonblocking(int fd);
void addfd_et(int epollfd, int fd, bool enable_et);
void addfd_oneshot(int epollfd, int fd, bool oneshot);
void reset_oneshot(int epollfd, int fd);
void addsig(int sig);
void removefd(int epollfd, int fd);
void modfd(int epollfd, int fd, int ev);

#endif
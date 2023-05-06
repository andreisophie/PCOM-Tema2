#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1024

enum tcp_action {
  CONNECT = 0,
  SUBSCRIBE = 1,
  UNSUBSCRIBE = 2,
  SHUTDOWN = 3,
};

struct __attribute__ ((__packed__)) tcp_header {
  enum tcp_action action;
  uint16_t len;
};

int send_tcp(int sockfd, struct tcp_header *header, void *body);
int recv_tcp(int sockfd, struct tcp_header *header, void *body);

#endif

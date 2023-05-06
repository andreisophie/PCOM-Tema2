#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 1024

enum tcp_action {
  SHUTDOWN = 0,
  SUBSCRIBE = 1,
  UNSUBSCRIBE = 2
};

struct __attribute__ ((__packed__)) tcp_packet {
  enum tcp_action action;
  uint16_t len;
  char message[MSG_MAXSIZE + 1];
};

int send_tcp(int sockfd, struct tcp_packet *buffer);
int recv_tcp(int sockfd, struct tcp_packet *buffer);

#endif

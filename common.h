#ifndef __COMMON_H__
#define __COMMON_H__

#include <stddef.h>
#include <stdint.h>
#include <sys/socket.h>

/* Dimensiunea maxima a mesajului */
#define MSG_MAXSIZE 2048
#define ARR_MAX 126

enum tcp_action {
    CONNECT = 0,
    SUBSCRIBE_SF = 1,
    SUBSCRIBE_NOSF = 2,
    MESSAGE = 3,
    UNSUBSCRIBE = 4,
    SHUTDOWN = 5,
};

struct __attribute__((__packed__)) tcp_header {
    enum tcp_action action;
    uint16_t len;
};

int send_tcp(int sockfd, struct tcp_header *header, void *body);
int recv_tcp(int sockfd, struct tcp_header *header, void *body);

struct topic {
    char topic[ARR_MAX];
    int sf;
};

typedef struct client_control_block CCB;
struct client_control_block {
    char id[MSG_MAXSIZE];
    int connected;
    int fd;
    int nr_topics;
    struct topic *topics;
    int nr_pending;
    char *pending_messages[ARR_MAX];
};

struct udp_message {
    char topic[51];
    uint8_t data_type;
    char body[1501];
};

#endif

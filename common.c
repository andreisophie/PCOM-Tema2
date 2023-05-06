#include "common.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int recv_tcp(int sockfd, struct tcp_header *header, void *body) {
    int msg_size = sizeof(struct tcp_header);
    size_t bytes_received = 0;
    size_t bytes_remaining = msg_size;
    char *buff = (void *)header;

    while(bytes_remaining) {
        int readbytes = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (readbytes < 0) {
            return -1;
        }
        bytes_received += readbytes;
        bytes_remaining -= readbytes;
    }

    int remaining_size = header->len;
    bytes_received = 0;
    bytes_remaining = remaining_size;
    buff = body;
    while(bytes_remaining) {
        int readbytes = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (readbytes < 0) {
            return -1;
        }
        bytes_received += readbytes;
        bytes_remaining -= readbytes;
    }

    return msg_size + remaining_size;
}

int send_tcp(int sockfd, struct tcp_header *header, void *body) {
    size_t msg_size = sizeof(struct tcp_header) + header->len;
    size_t bytes_sent = 0;
    size_t bytes_remaining = sizeof(struct tcp_header);
    char *buff = (void *)header;

    while(bytes_remaining) {
        int sendbytes = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (sendbytes < 0) {
            return -1;
        }
        bytes_sent += sendbytes;
        bytes_remaining -= sendbytes;
    }

    bytes_sent = 0;
    bytes_remaining = header->len;
    buff = body;
    while(bytes_remaining) {
        int readbytes = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (readbytes < 0) {
            return -1;
        }
        bytes_sent += readbytes;
        bytes_remaining -= readbytes;
    }

    return msg_size;
}
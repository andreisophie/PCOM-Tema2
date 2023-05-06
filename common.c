#include "common.h"

#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int recv_tcp(int sockfd, struct tcp_packet *packet) {
    int msg_size = 6;
    size_t bytes_received = 0;
    size_t bytes_remaining = msg_size;
    char *buff = (void *)packet;

    while(bytes_remaining) {
        int readbytes = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (readbytes < 0) {
            return -1;
        }
        bytes_received += readbytes;
        bytes_remaining -= readbytes;
    }

    int remaining_size = packet->len;
    bytes_remaining += remaining_size;
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

int send_tcp(int sockfd, struct tcp_packet *packet) {
    size_t msg_size = 6 + packet->len;
    size_t bytes_sent = 0;
    size_t bytes_remaining = msg_size;
    char *buff = (void *)packet;

    while(bytes_remaining) {
        int sendbytes = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (sendbytes < 0) {
            return -1;
        }
        bytes_sent += sendbytes;
        bytes_remaining -= sendbytes;
    }

    return msg_size;
}
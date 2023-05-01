#include "common.h"

#include <sys/socket.h>
#include <sys/types.h>

int recv_tcp(int sockfd, void *buffer, size_t len) {

    size_t bytes_received = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    while(bytes_remaining) {
        int readbytes = recv(sockfd, buff + bytes_received, bytes_remaining, 0);
        if (readbytes < 0) {
            return -1;
        }
        bytes_received += readbytes;
        bytes_remaining = len - bytes_received;
    }

    return len;
}

int send_tcp(int sockfd, void *buffer, size_t len) {
    size_t bytes_sent = 0;
    size_t bytes_remaining = len;
    char *buff = buffer;

    while(bytes_remaining) {
        int sendbytes = send(sockfd, buff + bytes_sent, bytes_remaining, 0);
        if (sendbytes < 0) {
            return -1;
        }
        bytes_sent += sendbytes;
        bytes_remaining = len - bytes_sent;
    }

    return len;
}
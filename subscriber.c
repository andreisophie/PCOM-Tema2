#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <poll.h>

#include "common.h"
#include "helpers.h"

void run_client(int sockfd) {
    char buf[MSG_MAXSIZE + 1];
    memset(buf, 0, MSG_MAXSIZE + 1);

    struct chat_packet sent_packet;
    struct chat_packet recv_packet;

    struct pollfd poll_fds[2];
    int num_clients = 2;
    int rc;

    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = sockfd;
    poll_fds[1].events = POLLIN;

    while (1) {
        rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");

        if (poll_fds[0].revents & POLLIN) {
            // primesc mesaj de la tastatura
            fgets(buf, sizeof(buf), stdin);
            if (isspace(buf[0])) {
                printf("Nu te mai juca cu spatiile!\n");
                continue;
            }
            sent_packet.len = strlen(buf) + 1;
            strcpy(sent_packet.message, buf);
            send_tcp(sockfd, &sent_packet, sizeof(sent_packet));
        } else if (poll_fds[1].revents & POLLIN) {
            // primesc mesaj de la alt client
            printf("Received message from another client:");
            int rc = recv_tcp(poll_fds[1].fd, &recv_packet, sizeof(recv_packet));
            if (rc <= 0) {
                printf("Eroare recv_all");
                break;
            }
            printf("%s\n", recv_packet.message);
        } else {
            printf("Nu stiu ce s-a intamplat bobita\n");
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd = -1;

    if (argc != 4) {
        printf("\n Usage: %s <id> <ip> <port>\n", argv[0]);
        return 1;
    }

    char *client_id = argv[1]; 

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[3], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    // Obtinem un socket TCP pentru conectarea la server
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(sockfd < 0, "socket");

    // Completăm in serv_addr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in serv_addr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    memset(&serv_addr, 0, socket_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    rc = inet_pton(AF_INET, argv[2], &serv_addr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Ne conectăm la server
    rc = connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(rc < 0, "connect");

    run_client(sockfd);

    // Inchidem conexiunea si socketul creat
    close(sockfd);

    return 0;
}

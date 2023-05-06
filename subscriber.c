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

void run_client(int sockfd, char *id) {
    char buf[MSG_MAXSIZE + 1];
    memset(buf, 0, MSG_MAXSIZE + 1);

    struct tcp_header header;

    // trimit la server mesaj cu id-ul meu
    header.action = CONNECT;
    strcpy(buf, id);
    header.len = strlen(id) + 1;
    send_tcp(sockfd, &header, buf);

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
            char command[MSG_MAXSIZE];
            char topic[MSG_MAXSIZE];
            int sf;
            int nr_fields = sscanf(buf, "%s %s %d", command, topic, &sf);
            if (nr_fields == 1 && !strcmp(command, "exit")) {
                // Deconectez clientul
                header.action = SHUTDOWN;
                header.len = 0;
                send_tcp(sockfd, &header, NULL);
                shutdown(sockfd, SHUT_RDWR);
                return;
            } else if (nr_fields == 3 && !strcmp(command, "subscribe") && (sf == 0 || sf == 1)) {
                // Dau subscribe
                if (sf == 0) {
                    header.action = SUBSCRIBE_NOSF;
                } else {
                    header.action = SUBSCRIBE_SF;
                }
                
                header.len = strlen(topic) + 1;
                send_tcp(poll_fds[1].fd, &header, (void *)topic);
                printf("Subscribed to topic.\n");
            } else if(nr_fields == 2 && !strcmp(command, "unsubscribe")) {
                // dau unsubscribe
                header.action = UNSUBSCRIBE;
                header.len = strlen(topic) + 1;
                send_tcp(poll_fds[1].fd, &header, &topic);
                printf("Unsubscribed from topic.\n");
            } else {
                printf("Unknown command\nUsage:\tsubscribe <topic> <sf>\n\tunsubscribe <topic>\n\texit\n");
            }
        } else if (poll_fds[1].revents & POLLIN) {
            // primesc mesaj de la server
            int rc = recv_tcp(poll_fds[1].fd, &header, buf);
            if (rc <= 0) {
                printf("Eroare recv_all");
                break;
            }
            if (header.action == SHUTDOWN) {
                shutdown(sockfd, SHUT_RDWR);
                return;
            } else if (header.action == MESSAGE) {
                printf("%s\n", buf);
            }
        } else {
            printf("Nu stiu ce s-a intamplat bobita\n");
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    int sockfd = -1;

    if (argc != 4) {
        printf("Usage: %s <id> <ip> <port>\n", argv[0]);
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

    run_client(sockfd, client_id);

    // Inchidem conexiunea si socketul creat
    close(sockfd);

    return 0;
}

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32
#define IP_SERVER "127.0.0.1"

int create_sock_udp(uint16_t port) {
    int udpfd;
    struct sockaddr_in udpservaddr;

    // Creating socket file descriptor
    if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    /* Make ports reusable, in case we run this really fast two times in a row */
    int enable = 1;
    if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
        perror("setsockopt(SO_REUSEADDR) failed");

    // Fill the details on what destination port should the
    // datagrams have to be sent to our process.
    memset(&udpservaddr, 0, sizeof(udpservaddr));
    udpservaddr.sin_family = AF_INET; // IPv4
    /* 0.0.0.0, basically match any IP */
    udpservaddr.sin_addr.s_addr = INADDR_ANY;
    udpservaddr.sin_port = htons(port);

    // Bind the socket with the server address. The OS networking
    // implementation will redirect to us the contents of all UDP
    // datagrams that have our port as destination
    int rc = bind(udpfd, (const struct sockaddr *)&udpservaddr, sizeof(udpservaddr));
    DIE(rc < 0, "bind failed");

    return udpfd;
}

int create_sock_tcp(uint16_t port) {
    // Obtinem un socket TCP pentru receptionarea conexiunilor
    int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcpfd < 0, "socket");

    // Completăm in tcpservaddr adresa serverului, familia de adrese si portul
    // pentru conectare
    struct sockaddr_in tcpservaddr;
    socklen_t socket_len = sizeof(struct sockaddr_in);

    // Facem adresa socket-ului reutilizabila, ca sa nu primim eroare in caz ca
    // rulam de 2 ori rapid
    int enable = 1;
    if (setsockopt(tcpfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0)
      perror("setsockopt(SO_REUSEADDR) failed");

    memset(&tcpservaddr, 0, socket_len);
    tcpservaddr.sin_family = AF_INET;
    tcpservaddr.sin_port = htons(port);
    int rc = inet_pton(AF_INET, IP_SERVER, &tcpservaddr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Asociem adresa serverului cu socketul creat folosind bind
    rc = bind(tcpfd, (const struct sockaddr *)&tcpservaddr, sizeof(tcpservaddr));
    DIE(rc < 0, "bind");

    return tcpfd;
}

void run_chat_multi_server(int udpfd, int tcpfd) {

    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_clients = 3;
    int rc;

    struct chat_packet received_packet;
    // Setam socket-ul tcpfd pentru ascultare
    rc = listen(tcpfd, MAX_CONNECTIONS);
    DIE(rc < 0, "listen");

    // se adauga noul file descriptor (socketul pe care se asculta conexiuni) in
    // multimea read_fds
    poll_fds[0].fd = STDIN_FILENO;
    poll_fds[0].events = POLLIN;
    poll_fds[1].fd = udpfd;
    poll_fds[1].events = POLLIN;
    poll_fds[2].fd = tcpfd;
    poll_fds[2].events = POLLIN;

    while (1) {
        rc = poll(poll_fds, num_clients, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_clients; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == STDIN_FILENO) {
                    // primesc mesaj de la tastatura, verific daca imi cere sa inchid server-ul
                    char buf[MSG_MAXSIZE + 1];
                    fgets(buf, sizeof(buf), stdin);
                    if (strcmp(buf, "exit")) {
                        // TODO: inchid toti clientii
                        close(udpfd);
                        close(tcpfd);

                        return 0;
                    }
                } else if (poll_fds[i].fd == udpfd) {
                    // TODO: primesc mesaj pe socket-ul udp
                } else if (poll_fds[i].fd == tcpfd) {
                    // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
                    // pe care serverul o accepta
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd =
                        accept(tcpfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // se adauga noul socket intors de accept() la multimea descriptorilor
                    // de citire
                    poll_fds[num_clients].fd = newsockfd;
                    poll_fds[num_clients].events = POLLIN;
                    num_clients++;

                    printf("Noua conexiune de la %s, port %d, socket client %d\n",
                        inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port),
                        newsockfd);
                } else {
                    // s-au primit date pe unul din socketii de client,
                    // asa ca serverul trebuie sa le receptioneze
                    int rc = recv_tcp(poll_fds[i].fd, &received_packet,
                                    sizeof(received_packet));
                    DIE(rc < 0, "recv");

                    if (rc == 0) {
                        // conexiunea s-a inchis
                        printf("Socket-ul client %d a inchis conexiunea\n", i);
                        close(poll_fds[i].fd);

                        // se scoate din multimea de citire socketul inchis
                        for (int j = i; j < num_clients - 1; j++) {
                            poll_fds[j] = poll_fds[j + 1];
                        }

                        num_clients--;

                    } else {
                        printf("S-a primit de la clientul de pe socketul %d mesajul: %s\n",
                                poll_fds[i].fd, received_packet.message);
                        /* Trimite mesajul catre toti ceilalti clienti */
                        for (int j = 2; j < num_clients; j++) {
                            if (j == i) continue;
                            send_tcp(poll_fds[j].fd, &received_packet, sizeof(received_packet));
                        }
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    if (argc != 2) {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int udpfd = create_sock_udp(port);
    int tcpfd = create_sock_tcp(port);

    run_chat_multi_server(udpfd, tcpfd);

    // Inchidem socketii
    close(udpfd);
    close(tcpfd);

    return 0;
}

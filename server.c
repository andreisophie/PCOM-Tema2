#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "helpers.h"

#define MAX_CONNECTIONS 32
#define IP_SERVER "127.0.0.1"

void remove_from_poll(struct pollfd *poll_fds, int *num_fds, int index) {
    for (int i = index; i < *num_fds - 1; i++) {
        poll_fds[i] = poll_fds[i + 1];
    }
    poll_fds[*num_fds].events = 0;
    (*num_fds)--;
}

int create_sock_udp(uint16_t port, struct sockaddr_in *udpservaddr) {
    int udpfd;

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
    memset(udpservaddr, 0, sizeof(*udpservaddr));
    udpservaddr->sin_family = AF_INET; // IPv4
    /* 0.0.0.0, basically match any IP */
    udpservaddr->sin_addr.s_addr = INADDR_ANY;
    udpservaddr->sin_port = htons(port);

    // Bind the socket with the server address. The OS networking
    // implementation will redirect to us the contents of all UDP
    // datagrams that have our port as destination
    int rc = bind(udpfd, (const struct sockaddr *)udpservaddr, sizeof(*udpservaddr));
    DIE(rc < 0, "bind failed");

    return udpfd;
}

int create_sock_tcp(uint16_t port) {
    // Obtinem un socket TCP pentru receptionarea conexiunilor
    int tcpfd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(tcpfd < 0, "socket");
    // dezactivez alg lui Nagle
    int yes = 1;
    int rc = setsockopt(tcpfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
    DIE(rc < 0, "Failed Nagle");

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
    rc = inet_pton(AF_INET, IP_SERVER, &tcpservaddr.sin_addr.s_addr);
    DIE(rc <= 0, "inet_pton");

    // Asociem adresa serverului cu socketul creat folosind bind
    rc = bind(tcpfd, (const struct sockaddr *)&tcpservaddr, sizeof(tcpservaddr));
    DIE(rc < 0, "bind");

    return tcpfd;
}

void add_topic(CCB *client, char *topic, int sf) {
    if (client->nr_topics == 0) {
        client->topics = malloc(sizeof(struct topic));
    } else {
        client->topics = realloc(client->topics, (client->nr_topics + 1) * sizeof(struct topic));
    }
    strcpy(client->topics[client->nr_topics].topic, topic);
    client->topics[client->nr_topics].sf = sf;
    client->nr_topics++;
}

struct topic *client_get_topic(CCB *client, char *topic) {
    for (int i = 0; i < client->nr_topics; i++) {
        if (!strcmp(topic, client->topics[i].topic)) {
            return &client->topics[i];
        }
    }
    return NULL;
}

void remove_topic(CCB *client, char *topic) {
    for (int i = 0; i < client->nr_topics; i++) {
        if (!strcmp(topic, client->topics[i].topic)) {
            for (int j = i; j < client->nr_topics - 1; j++) {
                client->topics[i] = client->topics[i + 1];
            }
            client->nr_topics--;
            client->topics = realloc(client->topics, client->nr_topics * sizeof(struct topic));
            return;
        }
    }
}

void run_chat_multi_server(int udpfd, int tcpfd, struct sockaddr_in *udpservaddr) {
    int num_clients = 0;
    CCB *clients = NULL;
    char buf[MSG_MAXSIZE + 1];
    memset(buf, 0, MSG_MAXSIZE + 1);
    struct pollfd poll_fds[MAX_CONNECTIONS];
    int num_fds = 3;
    int rc;

    struct tcp_header header;
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
        rc = poll(poll_fds, num_fds, -1);
        DIE(rc < 0, "poll");

        for (int i = 0; i < num_fds; i++) {
            if (poll_fds[i].revents & POLLIN) {
                if (poll_fds[i].fd == STDIN_FILENO) {
                    // primesc mesaj de la tastatura, verific daca imi cere sa inchid server-ul
                    fgets(buf, sizeof(buf), stdin);
                    if (!strncmp(buf, "exit", 4)) {
                        // inchid toti clientii
                        header.action = SHUTDOWN;
                        header.len = 0;
                        for (int i = 3; i < num_fds; i++) {
                            send_tcp(poll_fds[i].fd, &header, NULL);
                            shutdown(poll_fds[i].fd, SHUT_RDWR);
                            close(poll_fds[i].fd);
                        }
                        return;
                    }
                } else if (poll_fds[i].fd == udpfd) {
                    // primesc mesaj pe socket-ul udp
                    socklen_t size_udp = sizeof(*udpservaddr);

                    memset(buf, 0, MSG_MAXSIZE);
                    int ret = recvfrom(udpfd, buf, 1551, 0, (struct sockaddr *)udpservaddr, &size_udp);
                    DIE(ret < 0, "recvfrom failed");

                    struct udp_message recv_message;
                    memset(recv_message.topic, 0, 51);
                    strncpy(recv_message.topic, buf, 50);
                    recv_message.data_type = *((uint8_t *)(buf + 50));
                    memset(recv_message.body, 0, 1501);
                    memcpy(recv_message.body, buf + 51, 1500);
                    // Incep sa construiesc mesajul de trimis la clientii TCP
                    char buf2[ARR_MAX];
                    memset(buf, 0, MSG_MAXSIZE);
                    inet_ntop(AF_INET, &(udpservaddr->sin_addr), buf2, INET_ADDRSTRLEN);
                    strcpy(buf, buf2);
                    strcat(buf, ":");
                    memset(buf2, 0, ARR_MAX);
                    sprintf(buf2, "%d", ntohs(udpservaddr->sin_port));
                    strcat(buf, buf2);
                    strcat(buf, " - ");
                    strcat(buf, recv_message.topic);
                    strcat(buf, " - ");                    
                    uint8_t signum, power;
                    uint32_t value;
                    float float_value;
                    switch (recv_message.data_type) {
                        case 0:
                            // INT
                            signum = *((uint8_t *)recv_message.body);
                            value = ntohl(*((uint32_t *)(recv_message.body + 1)));
                            if (signum == 1)
                                value *= -1;
                            strcat(buf, "INT - ");
                            memset(buf2, 0, ARR_MAX);
                            sprintf(buf2, "%d", value);
                            strcat(buf, buf2);
                            break;
                        case 1:
                            // SHORT_REAL
                            value = ntohs(*((uint16_t *)recv_message.body));
                            float_value = (float)value / 100;
                            strcat(buf, "SHORT_REAL - ");
                            memset(buf2, 0, ARR_MAX);
                            sprintf(buf2, "%.2f", float_value);
                            strcat(buf, buf2);
                            break;
                        case 2:
                            // FLOAT
                            signum = *((uint8_t *)recv_message.body);
                            value = ntohl(*((uint32_t *)(recv_message.body + 1)));
                            power = *((uint8_t *)(recv_message.body + 5));
                            float_value = (float)value;
                            if (signum == 1)
                                float_value *= -1;
                            for (int i = 0; i < power; i++) {
                                float_value /= 10;
                            }
                            strcat(buf, "FLOAT - ");
                            memset(buf2, 0, ARR_MAX);
                            sprintf(buf2, "%.4f", float_value);
                            strcat(buf, buf2);                       
                            break;
                        case 3:
                            strcat(buf, "STRING - ");
                            strcat(buf, recv_message.body);  
                            break;
                    }
                    for (int j = 0; j < num_clients; j++) {
                        struct topic *client_topic = client_get_topic(&clients[j], recv_message.topic);
                        if (client_topic) {
                            if (clients[j].connected) {
                                header.action = MESSAGE;
                                header.len = strlen(buf) + 1;
                                send_tcp(clients[j].fd, &header, buf);
                            } else {
                                if (client_topic->sf == 1) {
                                    clients[j].pending_messages[clients[j].nr_pending++] = strdup(buf);
                                }
                            }
                            
                        }
                    }
                } else if (poll_fds[i].fd == tcpfd) {
                    // a venit o cerere de conexiune pe socketul inactiv (cel cu listen),
                    // pe care serverul o accepta
                    struct sockaddr_in cli_addr;
                    socklen_t cli_len = sizeof(cli_addr);
                    int newsockfd = accept(tcpfd, (struct sockaddr *)&cli_addr, &cli_len);
                    DIE(newsockfd < 0, "accept");

                    // se adauga noul socket intors de accept() la multimea descriptorilor
                    // de citire
                    poll_fds[num_fds].fd = newsockfd;
                    poll_fds[num_fds].events = POLLIN;
                    num_fds++;

                    // primesc de la subscriber mesaj cu id-ul lui
                    recv_tcp(newsockfd, &header, buf);
                    // creez un ccb pt noul client daca nu exista deja
                    if (clients == NULL) {
                        clients = malloc(sizeof(CCB));
                        clients[num_clients].connected = 1;
                        clients[num_clients].fd = newsockfd;
                        strcpy(clients[num_clients].id, buf);
                        num_clients++;
                    } else {
                        // caut daca exista deja un client cu id-ul resp
                        CCB *client = NULL;
                        for (int j = 0; j < num_clients; j++) {
                            if (!strcmp(clients[j].id, buf)) {
                                client = &clients[j];
                                break;
                            }
                        }
                        if (client != NULL) {
                            // daca exista deja, ii actualizez vechiul CCB
                            // verific daca este deja conectat un client pe acest ID
                            if (client->connected == 0) {
                                client->fd = newsockfd;
                                client->connected = 1;
                            } else {
                                // daca este un client duplicat, ii refuz conxiunea
                                printf("Client %s already connected.\n", buf);
                                header.action = SHUTDOWN;
                                header.len = 0;
                                send_tcp(newsockfd, &header, NULL);
                                remove_from_poll(poll_fds, &num_fds, num_fds - 1);
                                break;
                            }
                            
                            // verific daca am mesaje ramase de dinainte
                            for (int i = 0; i < client->nr_pending; i++) {
                                header.action = MESSAGE;
                                header.len = strlen(client->pending_messages[i]) + 1;
                                send_tcp(client->fd, &header, client->pending_messages[i]);
                            }
                        } else {
                            // daca nu exista, creez un CCB nou pentru el
                            clients = realloc(clients, (num_clients + 1) * sizeof(CCB));
                            strcpy(clients[num_clients].id, buf);
                            clients[num_clients].connected = 1;
                            clients[num_clients].fd = newsockfd;
                            clients[num_clients].nr_topics = 0;
                            clients[num_clients].nr_pending = 0;
                            
                            num_clients++;
                        }                        
                    }                  

                    printf("New client %s connected from %s:%d.\n",
                        buf, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
                    break;
                } else {
                    // s-au primit date pe unul din socketii de client,
                    // asa ca serverul trebuie sa le receptioneze
                    int rc = recv_tcp(poll_fds[i].fd, &header, buf);
                    DIE(rc < 0, "recv");

                    CCB *client = NULL;
                    for (int j = 0; j < num_clients; j++) {
                        if (poll_fds[i].fd == clients[j].fd) {
                            client = &clients[j];
                            break;
                        }
                    }

                    switch (header.action) {
                        case SHUTDOWN:
                            printf("Client %s disconnected.\n", client->id);
                            shutdown(poll_fds[i].fd, SHUT_RDWR);
                            close(poll_fds[i].fd);
                            client->connected = 0;
                            remove_from_poll(poll_fds, &num_fds, i);
                            break;
                        case SUBSCRIBE_NOSF:
                            add_topic(client, buf, 0);
                            break;
                        case SUBSCRIBE_SF:
                            add_topic(client, buf, 1);
                            break;
                        case UNSUBSCRIBE:
                            remove_topic(client, buf);
                            break;
                        default:
                            printf("this should not happen\n");
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    if (argc != 2) {
        printf("Usage: ./server <port>\n");
        return 0;
    }

    // Parsam port-ul ca un numar
    uint16_t port;
    int rc = sscanf(argv[1], "%hu", &port);
    DIE(rc != 1, "Given port is invalid");

    if (argc != 2) {
        printf("\n Usage: %s <port>\n", argv[0]);
        return 1;
    }

    struct sockaddr_in udpservaddr;
    int udpfd = create_sock_udp(port, &udpservaddr);
    int tcpfd = create_sock_tcp(port);

    run_chat_multi_server(udpfd, tcpfd, &udpservaddr);

    // Inchidem socketii
    close(udpfd);
    close(tcpfd);

    return 0;
}

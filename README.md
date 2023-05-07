# PCOM-Tema2
Made by Andrei Maruntis

## Functionarea aplicatiei

Aplicatia functioneaza astfel cum este prezentat in enuntul temei:

- se porneste server-ul cu o comanda de tipul `./server <port>`
    - ip-ul server-ului este mereu `127.0.0.1`
    - daca se incearca pornirea server-ului cu un numar gresit de parametrii in linia de comanda, se va afisa un mesaj sugestiv
- server-ul asteapta conexiuni de la clienti TCP sau mesaje de la clienti UDP
- clientii TCP se pornesc cu o comanda de tipul `./subscriber <id> <server_ip> <port>`
    - daca se incearca pornirea clientilor cu un numar gresit de parametrii in linia de comanda, se va afisa un mesaj sugestiv
    - clientii accepta cele 3 tipuri de comenzi descrise in enunt:
        - `subscribe <topic> <sf>`
        - `unsubscribe <topic>`
        - `exit`
    - clientul va afisa un mesaj de eroare, precum si instructiuni de folosire, daca se introuce o alta comanda decat cele de mai sus (nume diferit sau numar gresit de parametrii)
- server-ul va trimite mesaje clientilor TCP asa cum este explicat in enunt (mesajele primite de la clientii UDP pe diverse topic-uri ajung la clientii TCP care s-au abonat la acele topic-uri)
- la inchiderea server-ului, este trimis un pachet de shutdown tutror clientilor conectati, care se vor inchide si ei

## Protocolul TCP

Pentru comunicarea intre clientii TCP folosesc un protocol simplu, dar eficient, care functioneaza astfel:

- Orice mesaj TCP are un header cu urmatoarele campuri:

| Field  | Datatype        | Size  | Info                          |
|--------|-----------------|-------|-------------------------------|
| action | `enum tcp_action` | 4 oct | tipul actiunii                |
| len    | `uint16_t`        | 2 oct | lungimea datelor suplimentare |

Campul `enum tcp_action` accepta urmatoarele tipuri de actiuni:

```
enum tcp_action {
    CONNECT = 0,
    SUBSCRIBE_SF = 1,
    SUBSCRIBE_NOSF = 2,
    MESSAGE = 3,
    UNSUBSCRIBE = 4,
    SHUTDOWN = 5,
};
```

Astfel, header-ul:

- are exact 6 octeti
- transmite informatii despre tipul actiunii
- in functie de tipul actiunii, se pot transmite date suplimentare:
    - actiunea de tipul `CONNECT` va atasa header-ului un string care reprezinta id-ul clientului care tocmai s-a conectat
    - actiunile de tipul `SUBSCRIBE_NOSF`, `SUBSCRIBE_SF` si `UNSUBSCRIBE` vor avea atasat un string care reprezinta topic-ul la care clientul se aboneaza
    - actiunile de tipul `MESSAGE` vor avea atasat un string care reprezinta mesajul primit de la clientii UDP
- pentru tipurile de mesaje descrise mai sus, campul `len` va contine lungimea string-urilor atasate; pentru tipurile de mesaje care nu transmit date suplimentare (`SHUTDOWN`) acest camp va avea valoarea 0

In plus, pentru a preveni fragmentarea mesajelor TCP, atat server-ul, cat si clientii vor asculta pana cand primesc exact numarul de octeti necesar:

- vor astepta mai intai exact 6 octeti pentru header
- in functie de lungimea de date mentioneata in header (campul `len`), vor astpta octeti suplimentari

Astfel, protocolul TCP este eficient din urmatoarele motive:

- are un header de lungime mica (6 octeti) care contine informatii relevante
- date suplimentare se trimit doar in cazul in care sunt necesare si au lungimea minima, mentionata in header
- clientii primesc numarul minim de mesaje (primesc doar mesajele de la clientii UDP adresate topic-urilor la care au dat subscribe), mai multe detalii mai jos

## Functionarea protocolului TCP

In aceasta sectiune voi descrie in detaliu functionarea protocolului TCP pentru fiecare tip de mesaj

1. `CONNECT`

- Acest tip de mesaj este trimis doar de subscriberi catre server la conectare
- Conectarea se realizeaza astfel:
    - la deschiderea procesului subscriber se initiaza conexiunea prin intermediul API-ului de sockets
    - server-ul va primi cererea de conectare pe socket-ul pe care face listen pentru cererei TCP noi, apoi va astepta un mesaj TCP de tipul `CONNECT` de la subscriber care va contine id-ul ca date suplimentare (dupa header)
    - server-ul va verifica daca clientul s-a mai conectat si, in cazul in care exista deja un client conectat cu id-ul dat de subscriber, va inchide conexiunea (va trimite un pachet TCP cu actiunea `SHUTDOWN`)

2. `SUBSCRIBE_SF`

- Acest tip de mesaj este folosit doar de subscriberi catre server atunci cand primesc de la tastatura o comanda de tipul `subscribe` cu `sf=1`
- Ca date suplimentare clientii vor trimite numele topic-ului la care se aboneaza
- Server-ul va procesa cererea subscriber-ului, mai multe detalii mai jos

3. `SUBSCRIBE_NOSF`

- Identic cu `SUBSCRIBE_SF` doar ca se initiaza pentru comenzi de subscribe cu `sf=1`
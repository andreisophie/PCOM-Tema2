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

```C
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
    - actiunile de tipul `SUBSCRIBE_NOSF`, `SUBSCRIBE_SF` si `UNSUBSCRIBE` vor avea atasat un string care reprezinta topic-ul la care clientul se aboneaza/ dezaboneaza
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

4. `MESSAGE`

- Acest tip de mesaj este trimis doar de catre server subscriberilor
- Datele suplimentare reprezinta string-ul care contine
mesajul primit de la un client UDP
- Serverul se asigura ca numai clientii abonati la un anumit topic vor primi astfel de mesaje, deci subscriberul nu trebuie sa verifice nimic cand primeste un asemenea mesaj

5. `UNSUBSCRIBE`

- Identic cu comenzide te tip `SUBSCRIBE`, doar ca este folosit pentru a dezabona un subscriber de la un anumit topic si lipseste parametrul `sf`

6. `SHUTDOWN`

- Acest tip de mesaj poate fi trimis atat de server subscriberilor, cat si invers
- In functie de cine il trimite, mesajul are semnificatii diferite:
    - daca este trimis de **server**, acesta reprezinta o notificare catre toti clientii ca serverul se inchide si ca trebuie sa se inchida si ei
    - daca este trimis de **client**, acesta reprezinta o notificare ca respectivul client se va inchide si server-ul trebuie sa il marcheze intern ca deconectat, in pregatire pentru o viitoare reconectare

## Functionarea server-ului

In aceasta sectiune voi descrie in detaliu functionarea server-ului, structurile de date de care se foloseste, etc.

- La pornire, server-ul deschide un socket UDP si un socket TCP pe adresa `127.0.0.1` si port-ul primit ca parametru in linia de comanda, ambii pe acelasi port
- Cei doi socketi nou deschisi vor asculta mesaje (UDP), respectiv cereri noi de conexiune (TCP)
- Dupa pornirea cu succes a socketilor, serverul trebuie sa monitoreze conexiuni atat pe cei doi socketi, cat si de la tastatura; pentru asta folosesc API-ul `poll`
- La conectarea fiecarui subscriber nou, serverul va astepta ca acesta sa ii trimita un mesaj TCP cu ID-ul sau (cum am descris mai sus), deoarece aceasta informatie este esentiala pentru configurarea ulterioara a structurilor interne
    - Pentru a gestiona clientii, server-ul asociaza fiecarui subscriber un Client Control Block (CCB) care contine urmatoarele date:
        - file descriptor-ul pe care clientul primeste si trimite mesaje
        - id-ul clientului
        - daca clientul este conectat sau nu la momentul curent
        - topic-urile la care clientul este abonat
        - mesajele pe care clientul trebuie sa le primeasca cand se va reconecta, datorita functionalitatii de `store-and-forward`
    - Dupa ce primeste id-ului unui client care doreste sa se conecteze, server-ul verifica urmatoarele lucruri:
        - are serverul in memorie un CCB asociat acestui client?
            - daca da, verifica daca este deja un client cu acel id conectat
                - daca da, refuza conexiunea si trimite un mesaj de tip `SHUTDOWN` subscriber-ului
                - altfel, actualizeaza CCB-ul gasit si trimite orice mesaje pending catre subscriber
            - altfel, creeaza un CCB nou pentru acel subscriber
- Subscriberii deja conectati au posibilitatea de a se abona/ dezabona la/ de la topic-uri, server-ul va primi astfel de mesaje de la clienti si va actualiza CCB-ul asociat clientului in mod corespunzator
- Atunci cand primeste un mesaj de la un client UDP, server-ul va compune mesajul (string-ul) de trimis catre clientii UDP si va parcurge lista de CCBs in cautarea clientilor care sunt abonati la topic-ul primit de la clientii UDP
    - La gasirea unui client abonat, serverul fie va trimite imediat mesajul catre clientul TCP daca acesta este conectat, fie il va pune in coada de mesaje de trimis daca clientul este deconectat si abonamentul sau la topic-ul cautat este cu flag-ul `sf=1`
- La deconectarea unui subscriber, serverul va primi mesajul de tip `SHUTDOWN` de la client si va marca acel subscriber ca fiind deconectat in CCB-ul sau si ii va sterge file descriptor-ul din lista de polling, fiind pregatit pentru o eventuala reconectare

## Alte observatii

Tema aceasta a fost interesanta, insa enuntul a lasat putin de dorit din anumite puncte de vedere. Astfel, ofer urmatorul feedback:

- Ar trebui scris in clar in enuntul temei ca IP-ului pe care trebuie sa porneasca serverul este `127.0.0.1` si faptul ca ambii socketi (UDP si TCP) vor porni pe acelasi port
- As dori o explicatie mai clara pentru ce inseamna *definirea unui protocol eficient de nivel aplicatie peste TCP*
    - avand in vedere ce am invatat pana acum la aceasta materie, la prima vedere am crezut ca se doreste crearea unui protocol peste TCP cu ACK-uri sau ceva asemanator
    - cred ca ar fi bine de evitat cuvantul protocol in aceasta formulare, pentru ca de fapt se cere ceva mult mai simplu decat un protocol cum am explicat mai sus (o structura eficienta a mesajelor/ evitarea transmiterii unor mesaje inutile)
- Nu ar deranja pe nimeni daca ne-ati scuti de cautat pe google cum sa dezactivam alg lui Nagle 😁 (din paginile unde am cautat aceasta informatie, 4 din 5 aveau informatii gresite)

In rest, cateva lucruri pe care le-am apreciat la aceasta tema:

- Nu a trebuit sa folosim Mininet ❤
- Checker-ul nu ofera punctaje neaparat, sugerand ca oferirea notelor se va face mai degraba de mana, ceea ce mi se pare mai potrivit la aceasta materie unde lucram cu notiuni si API-uri complexe care pot merge prost in extrem de multe feluri
- Posibilitatea de a refolosi foarte mult cod deja lucrat la laboratoare, dar in aceeasi masura crearea de cod noupentru functionalitati noi
- Tema avea putin din toate, abordeaza mai multe concepte la un nivel destul de basic, facand-o destul de facila si accesibila, dar avand in continuare posibilitatea de a invata extrem de multe chestii din ea
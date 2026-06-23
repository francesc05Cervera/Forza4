#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "game.h"

#define PORT 9090 //definisce la porta su cui il server ascolta
#define MAX_CLIENT 100 //definisce il numero massimo di client connessi

int client_connessi[MAX_CLIENT]; //Array per memorizzare i socket dei client connessi
int num_client_connessi = 0; //Numero di client attualmente connessi

/*
Funzione per inviare una notifica a tutti i client connessi tranne quelli della partita specificata
*/
void broadcast_notifica(int id_partita, char *msg) 
{
    pthread_mutex_lock(&lock_globale); //blocca il mutex globale per proteggere l'accesso all'array dei client connessi
    for (int i = 0; i < num_client_connessi; i++)  //scorro l'array dei client connessi
    {
        int s = client_connessi[i]; //per ogni client connesso
        if (s != partite[id_partita].socket_g1 && s != partite[id_partita].socket_g2)  
        {//verifico che il socket del client non sia quello dei giocatori della partita specificata
            write(s, msg, strlen(msg)); //se non lo è, invio la notifica al client
        }
    }
    pthread_mutex_unlock(&lock_globale); //sblocca il mutex globale dopo aver finito di inviare le notifiche
}


/*
Funzione per gestire i client connessi 
*/
void *gestisci_client(void *socket_desc) //riceve in input il socket del client connesso
{
    int sock = *(int*)socket_desc; //cast del puntatore a intero per ottenere il socket del client
    free(socket_desc); //libera la memoria allocata per il puntatore al socket
    
    char buffer[1024]; //alloco un buffer per lettura dei messaggi ricevuti dal client
    char risposta[1024]; //un'altro per le risposte da inviare al client
    int mio_id_partita = -1; //variabile per mem l'id della paritta
    int mio_giocatore = 0; //variabile per memorizzare il numero del giocatore (1 o 2)

    while(1)  //ciclo infinito per gestire i messaggi ricevuti dal client
    {
        memset(buffer, 0, 1024); //azzero il buffer prima di leggere un nuovo messaggio
        int read_size = read(sock, buffer, 1024); //inizio a leggere il messaggio inviato dal client e lo memorizzo nel buffer
        
        if (read_size <= 0) //se la lettura fallisce o il client si disconnette
        {
            printf("Client disconnesso\n"); //mostra a schermo che il client si è disconnesso
            break; //interrompe il while e termina la gestione del client
        }

        printf("Ricevuto: %s\n", buffer); //stampa ciò che viene ricevuto dal client

        if (strncmp(buffer, "CREATE", 6) == 0) //se il messaggio ricevuto è "CREATE" allora creo una nuova partita
        {
            mio_id_partita = crea_partita(sock); //uso la funzione crea_partita di game.h e memorizzo l'id della partita
            if (mio_id_partita == -1) //se l'ID è -1 vuol dire che c'è stato un problema 
            {
                write(sock, "ERRORE;Nessuna partita disponibile\n", 35); //invia al client un messaggio di errore
                continue; //continua il ciclo per attendere un nuovo messaggio dal client
            }

            /* Se Va a Buon Fine */
            mio_giocatore = 1; //memorizzo che il giocatore è il primo a giocare (quello che crea la partita)
            sprintf(risposta, "PARTITA_CREATA;%d\n", mio_id_partita); //preparo la risposta da inviare 
            write(sock, risposta, strlen(risposta)); //invio

            char msg_notifica[100]; //array per le notifiche da inviare agli altri client connessi
            sprintf(msg_notifica, "NOTIFICA;La partita ID %d e' stata creata ed e' in attesa di giocatori.\n", mio_id_partita); //preparo
            broadcast_notifica(mio_id_partita, msg_notifica); //invio
        }
        else if (strncmp(buffer, "JOIN", 4) == 0) 
        { //se il messaggio ricevuto è "JOIN" allora il client vuole unirsi a una partita esistente
            int id_richiesto; //variabile per memorizzare l'id della partita a cui il client vuole unirsi
            sscanf(buffer, "JOIN;%d", &id_richiesto); //leggo l'id della partita dal messaggio ricevuto
            
            if (unisciti_partita(id_richiesto, sock)) 
            { //se la funzione unisciti_partita restituisce 1 vuol dire che il client si è unito con successo alla partita
                char richiesta[50]; //array per preparare la richiesta da inviare al giocatore 1 della partita
                sprintf(richiesta, "RICHIESTA_JOIN;%d\n", sock); //preparo la richiesta da inviare al giocatore 1 della partita
                write(partite[id_richiesto].socket_g1, richiesta, strlen(richiesta)); //invio
                
                mio_id_partita = id_richiesto; //memorizzo l'id della partita a cui il client si è unito
                mio_giocatore = 2; //memorizzo che il giocatore è il secondo a giocare (quello che si unisce alla partita)
            } else {
                write(sock, "ERRORE;Partita non disponibile o inesistente\n", 45); //invia al client un messaggio di errore se la partita non è disponibile o inesistente
            }
        }
        else if (strncmp(buffer, "RISPOSTA_JOIN", 13) == 0)
        { //se il messaggio ricevuto è "RISPOSTA_JOIN" allora il giocatore 1 della partita sta rispondendo alla richiesta di un giocatore 2
            if (mio_id_partita < 0) 
            { //se l'id della partita è minore di 0 vuol dire che non c'è nessuna partita attiva
                write(sock, "ERRORE;Nessuna partita attiva\n", 30);
                continue;
            }
            char esito[10];
            sscanf(buffer, "RISPOSTA_JOIN;%9s", esito); //leggo l'esito della risposta dal messaggio ricevuto
            
            if (strcmp(esito, "SI") == 0)
             { //se l'esito della risposta è "SI" allora il giocatore 1 ha accettato la richiesta del giocatore 2
                gestisci_risposta_join(mio_id_partita, 1); //chiamo la funzione gestisci_risposta_join per aggiornare lo stato della partita e notificare il giocatore 2
                char msg_ok[50];
                sprintf(msg_ok, "UNISCITI_OK;%d\n", mio_id_partita); //preparo il messaggio di conferma da inviare al giocatore 2
                write(partite[mio_id_partita].socket_g2, msg_ok, strlen(msg_ok)); //invio

                char msg_notifica[100];
                sprintf(msg_notifica, "NOTIFICA;La partita ID %d e' iniziata. Non e' piu' possibile partecipare.\n", mio_id_partita);
                //invio la notifica a tutti i client connessi tranne quelli della partita specificata
                broadcast_notifica(mio_id_partita, msg_notifica);
                //uso del metodo broadcast_notifica per inviare la notifica a tutti i client connessi tranne quelli della partita specificata
            } else {
                /* gestione rifiuto */
                int socket_rifiutato = partite[mio_id_partita].socket_g2; //memorizzo il socket del giocatore 2 che ha ricevuto il rifiuto
                gestisci_risposta_join(mio_id_partita, 0); //chiamo la funzione gestisci_risposta_join per aggiornare lo stato della partita e notificare il giocatore 2
                write(socket_rifiutato, "ERRORE;Il creatore ha rifiutato la tua richiesta.\n", 51); //invio al giocatore 2 un messaggio di errore per notificare il rifiuto della richiesta
                mio_id_partita = -1; //assegno -1 all'id della partita per indicare che non c'è nessuna partita attiva
                mio_giocatore = 0; //0 perchè non c'è nessuna partita attiva
            }
        }
        else if (strncmp(buffer, "MOVE", 4) == 0) /*Move: serve a gestire i movimenti dei giocatori */
        {
            if (mio_id_partita < 0) { //se non ci sono partite
                write(sock, "ERRORE;Nessuna partita attiva\n", 30); //lo notifico....
                continue; //e continuo...
            }
            int colonna; //colonna in cui il giocatore vuole inserire il gettone
            sscanf(buffer, "MOVE;%d", &colonna); //recupero il valore della colonna dal messaggio ricevuto
            
            int riga = inserisci_gettone(mio_id_partita, mio_giocatore, colonna);
            //uso il metodo inserisci_gettone per inserire il gettone nella griglia della partita e memorizzo la riga in cui è stato inserito
            
            if (riga != -1) { //se la riga è diversa da -1 vuol dire che il gettone è stato inserito correttamente
                char msg_update[50]; //array per preparare il messaggio di aggiornamento da inviare ai giocatori della partita
                sprintf(msg_update, "UPDATE;%d;%d;%d\n", riga, colonna, mio_giocatore);
                //preparo il messaggio di aggiornamento da inviare ai giocatori della partita con le coordinate del gettone inserito e il numero del giocatore
                write(partite[mio_id_partita].socket_g1, msg_update, strlen(msg_update)); //invio il msg a giocatore 1
                write(partite[mio_id_partita].socket_g2, msg_update, strlen(msg_update)); //e a giocatore 2
                
                int vincitore = controlla_vittoria(mio_id_partita); //controllo se c'è un vincitore o un pareggio

                if (vincitore != 0) //se c'è un vincitore o un pareggio
                {
                    partite[mio_id_partita].stato = TERMINATA; //dico che la partita è finita
                    
                    if (vincitore == 3) //se è = 3 vuol dire che c'è stato un pareggio
                    {
                        write(partite[mio_id_partita].socket_g1, "PAREGGIO\n", 9); //lo dico a giocatore 1
                        write(partite[mio_id_partita].socket_g2, "PAREGGIO\n", 9); // e a giocatore 2
                    } 
                    else if (vincitore == 1) //se è = 1 vuol dire che ha vinto il giocatore 1
                    {
                        write(partite[mio_id_partita].socket_g1, "VITTORIA\n", 9); //glielo comunico (FC INTER)
                        write(partite[mio_id_partita].socket_g2, "SCONFITTA\n", 10); //giocatore 2 ha perso (SSCNAPOLI)
                    } 
                    else if (vincitore == 2) 
                    {//se è = 2 vuol dire che ha vinto il giocatore 2
                        write(partite[mio_id_partita].socket_g1, "SCONFITTA\n", 10); //giocatore 1 ha perso (SSCNAPOLI)
                        write(partite[mio_id_partita].socket_g2, "VITTORIA\n", 9); //giocatore 2 ha vinto (FC INTER)
                    }

                    /* COMUNICO IN BROADCAST CHE LA PARTITA È CONCLUSA */
                    char msg_notifica[100];
                    sprintf(msg_notifica, "NOTIFICA;La partita ID %d si e' conclusa.\n", mio_id_partita);
                    broadcast_notifica(mio_id_partita, msg_notifica);
                }
            } else {
                write(sock, "MOSSA_NON_VALIDA\n", 17); //se la mossa non è valida invio un messaggio di errore al client
            }
        }
        else if (strncmp(buffer, "REMATCH", 7) == 0) //richiesta di Rivincita da parte di un giocatore
        {
            if (mio_id_partita < 0) { //verifica di esistenza di una partita attiva
                write(sock, "ERRORE;Nessuna partita attiva\n", 30);
                continue;
            }
            pthread_mutex_lock(&partite[mio_id_partita].lock_partita);
            //blocco il mutex delle partite per assicurarmi che nessun altro thread possa modificare lo stato della partita mentre sto gestendo la richiesta di rivincita
            if (partite[mio_id_partita].stato == TERMINATA) 
            { //se la partita è terminata posso valutare la rivincita
                partite[mio_id_partita].voti_rematch++;  //conto i voti, se è 2 vuol dire che la rivincita è accettata
                if (partite[mio_id_partita].voti_rematch == 2) { 
                    pthread_mutex_unlock(&partite[mio_id_partita].lock_partita); //sblocco il mutex perchè ho finito
                    reset_griglia(mio_id_partita); //resetto la griglia della partita per iniziare una nuova partita
                    write(partite[mio_id_partita].socket_g1, "REMATCH_OK\n", 11); //do conferma a g1
                    write(partite[mio_id_partita].socket_g2, "REMATCH_OK\n", 11); // e a g2

                    /* comunico in broadcast la rivincita */
                    char msg_notifica[100];
                    sprintf(msg_notifica, "NOTIFICA;I giocatori della partita ID %d hanno accettato la rivincita! Il match ricomincia.\n", mio_id_partita);
                    broadcast_notifica(mio_id_partita, msg_notifica);
                } else {
                    pthread_mutex_unlock(&partite[mio_id_partita].lock_partita);
                    //se la partita non è finita, sblocco il mutex e attedo...
                }
            } else {
                pthread_mutex_unlock(&partite[mio_id_partita].lock_partita);
            }
        }
        else if (strncmp(buffer, "NO_REMATCH", 10) == 0) 
        { //gestione del rifiuto della rivincita da parte di un giocatore
            if (mio_id_partita < 0) {
                write(sock, "ERRORE;Nessuna partita attiva\n", 30); //solita verifica di esistenza partita attiva
                continue;
            }
            /* FIX: lettura di altro_socket dentro il mutex per evitare race condition */
            pthread_mutex_lock(&partite[mio_id_partita].lock_partita); //blocco il mutex delle partite per assicurarmi che nessun altro thread possa modificare lo stato della partita mentre sto gestendo la richiesta di rivincita
            int altro_socket = (mio_giocatore == 1) //se il giocatore è il primo a giocare, allora l'altro socket è quello del secondo giocatore, altrimenti è quello del primo giocatore
                ? partite[mio_id_partita].socket_g2
                : partite[mio_id_partita].socket_g1; //determino il socket dell'altro giocatore della partita
            
            partite[mio_id_partita].stato = LIBERA; //imposto lo stato della partita a LIBERA per indicare che la partita è terminata e può essere riutilizzata
            pthread_mutex_unlock(&partite[mio_id_partita].lock_partita); //sblocco il mutex delle partite dopo aver modificato lo stato della partita

            write(altro_socket, "ABBANDONO\n", 10); //invio un messaggio all'altro giocatore per notificare che l'altro giocatore ha rifiutato la rivincita

            /* invio msg in broadcast */
            char msg_notifica[100];
            sprintf(msg_notifica, "NOTIFICA;La stanza della partita ID %d e' stata chiusa ed e' di nuovo libera.\n", mio_id_partita);
            broadcast_notifica(mio_id_partita, msg_notifica);
        }
    }

    pthread_mutex_lock(&lock_globale);  //blocco il mutex globale per proteggere l'accesso all'array dei client connessi
    for (int i = 0; i < num_client_connessi; i++) { //scorro l'array dei client connessi
        if (client_connessi[i] == sock) { //se trovo il socket del client che si è disconnesso
            for (int j = i; j < num_client_connessi - 1; j++) { //sposto tutti gli elementi successivi di una posizione indietro per rimuovere il socket del client disconnesso
                client_connessi[j] = client_connessi[j + 1]; //sposto l'elemento successivo in posizione j
            }
            num_client_connessi--; //decremento il numero di client connessi
            break;
        }
    }
    pthread_mutex_unlock(&lock_globale); //sblocco

    close(sock); //chiudo il socket del client disconnesso
    return NULL; //termino il thread che gestiva il client disconnesso
}

int main() {
    int server_fd, new_socket; //socket del server e del nuovo client connesso
    struct sockaddr_in address; //struttura per memorizzare l'indirizzo del server e del client
    int opt = 1; //opzione per il socket per riutilizzare l'indirizzo e la porta
    int addrlen = sizeof(address); //dimensione della struttura address

    inizializza_partite(); //inizializzo le partite disponibili

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) //creo il socket del server
    { //se ritorna 0 c'è stato un errore nella creazione del socket
        perror("Creazione socket fallita"); 
        exit(EXIT_FAILURE); //e blocco tutto
    }

    /* setto le opzioni del socket */
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR fallita"); 
        exit(EXIT_FAILURE);
    }

    #ifdef SO_REUSEPORT 
    //SO_REUSEPORT permette a più socket di ascoltare sulla stessa porta, utile per bilanciare il carico tra più thread o processi.
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT fallita"); 
        exit(EXIT_FAILURE);
    }
    #endif

    address.sin_family = AF_INET; //AF_INET indica che il socket utilizza il protocollo IPv4
    address.sin_addr.s_addr = INADDR_ANY; //INADDR_ANY indica che il socket accetta connessioni da qualsiasi indirizzo IP
    address.sin_port = htons(PORT); //htons converte il numero di porta da host byte order a network byte order (big-endian)

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) { //bind associa il socket del server all'indirizzo e alla porta specificati
        perror("Bind fallito"); 
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 10) < 0) { //listen mette il socket del server in modalità di ascolto per le connessioni in arrivo, con una coda massima di 10 connessioni in attesa
        perror("Listen fallito"); 
        exit(EXIT_FAILURE);
    }

    printf("Server C Completato! In attesa sulla porta %d...\n", PORT); //tutto pronto!

    while(1) { //ciclo infinito per accettare connessioni dai client
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) { 
            //accept accetta una connessione in arrivo e crea un nuovo socket per comunicare con il client, se fallisce ritorna -1
            perror("Accept fallito");
            continue;
        }

        /* FIX: bounds check prima di inserire nell'array client_connessi */
        pthread_mutex_lock(&lock_globale); //blocco il mutex globale per proteggere l'accesso all'array dei client connessi
        if (num_client_connessi < MAX_CLIENT) //si verifica che i client connessi non superino il numero massimo consentito
        {
            client_connessi[num_client_connessi++] = new_socket; //se non lo supera, aggiungo il nuovo socket del client connesso all'array dei client connessi e incremento il numero di client connessi
            pthread_mutex_unlock(&lock_globale); //sblocco tutto
        } else {
            pthread_mutex_unlock(&lock_globale); //sblocco cmq altrimenti
            write(new_socket, "ERRORE;Server pieno\n", 20); //e dico che il server e pieno
            close(new_socket); //chiudo la socket del client connesso
            continue; //continuo il ciclo per accettare nuove connessioni
        }

        int *new_sock = malloc(sizeof(int)); //alloco dinamicamente la memoria per il nuovo socket del client connesso
        *new_sock = new_socket; //assegno il valore del nuovo socket del client connesso alla memoria allocata
        pthread_t sn_thread; //creo un nuovo thread per gestire il client connesso, passando il puntatore al socket del client come argomento
        
        if (pthread_create(&sn_thread, NULL, gestisci_client, (void*) new_sock) < 0) 
        { //se la creazione del thread fallisce, stampo un messaggio di errore e libero la memoria allocata per il socket del client connesso 
            perror("Errore thread");
            free(new_sock);
            continue;
        }
        pthread_detach(sn_thread); //dopo aver creato il thread, lo stacco dal thread principale in modo che possa terminare autonomamente quando il client si disconnette, senza dover essere joinato
    }
    
    close(server_fd);
    return 0;
}
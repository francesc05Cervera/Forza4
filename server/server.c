#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "game.h"

#define PORT 9090

int client_connessi[100];
int num_client_connessi = 0;

void broadcast_notifica(int id_partita, char *msg) {
    pthread_mutex_lock(&lock_globale);
    for (int i = 0; i < num_client_connessi; i++) {
        int s = client_connessi[i];
        if (s != partite[id_partita].socket_g1 && s != partite[id_partita].socket_g2) {
            write(s, msg, strlen(msg));
        }
    }
    pthread_mutex_unlock(&lock_globale);
}

void *gestisci_client(void *socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);
    
    char buffer[1024];
    char risposta[1024];
    int mio_id_partita = -1;
    int mio_giocatore = 0;

    while(1) {
        memset(buffer, 0, 1024);
        int read_size = read(sock, buffer, 1024);
        
        if (read_size <= 0) {
            printf("Client disconnesso\n");
            break;
        }

        printf("Ricevuto: %s\n", buffer);

        if (strncmp(buffer, "CREATE", 6) == 0) {
            mio_id_partita = crea_partita(sock);
            mio_giocatore = 1;
            sprintf(risposta, "PARTITA_CREATA;%d\n", mio_id_partita);
            write(sock, risposta, strlen(risposta));

            char msg_notifica[100];
            sprintf(msg_notifica, "NOTIFICA;La partita ID %d e' stata creata ed e' in attesa di giocatori.\n", mio_id_partita);
            broadcast_notifica(mio_id_partita, msg_notifica);
        }
        else if (strncmp(buffer, "JOIN", 4) == 0) {
            int id_richiesto;
            sscanf(buffer, "JOIN;%d", &id_richiesto);
            
            if (unisciti_partita(id_richiesto, sock)) {
                char richiesta[50];
                sprintf(richiesta, "RICHIESTA_JOIN;%d\n", sock); 
                write(partite[id_richiesto].socket_g1, richiesta, strlen(richiesta));
                
                mio_id_partita = id_richiesto;
                mio_giocatore = 2;
            } else {
                write(sock, "ERRORE;Partita non disponibile o inesistente\n", 45);
            }
        }
        else if (strncmp(buffer, "RISPOSTA_JOIN", 13) == 0) {
            char esito[10];
            sscanf(buffer, "RISPOSTA_JOIN;%s", esito);
            
            if (strcmp(esito, "SI") == 0) {
                gestisci_risposta_join(mio_id_partita, 1);
                char msg_ok[50];
                sprintf(msg_ok, "UNISCITI_OK;%d\n", mio_id_partita);
                write(partite[mio_id_partita].socket_g2, msg_ok, strlen(msg_ok));

                char msg_notifica[100];
                sprintf(msg_notifica, "NOTIFICA;La partita ID %d e' iniziata. Non e' piu' possibile partecipare.\n", mio_id_partita);
                broadcast_notifica(mio_id_partita, msg_notifica);
            } else {
                int socket_rifiutato = partite[mio_id_partita].socket_g2;
                gestisci_risposta_join(mio_id_partita, 0);
                write(socket_rifiutato, "ERRORE;Il creatore ha rifiutato la tua richiesta.\n", 51);
                mio_id_partita = -1;
                mio_giocatore = 0;
            }
        }
        else if (strncmp(buffer, "MOVE", 4) == 0) {
            int colonna;
            sscanf(buffer, "MOVE;%d", &colonna);
            
            int riga = inserisci_gettone(mio_id_partita, mio_giocatore, colonna);
            
            if (riga != -1) { 
                char msg_update[50];
                sprintf(msg_update, "UPDATE;%d;%d;%d\n", riga, colonna, mio_giocatore);
                
                write(partite[mio_id_partita].socket_g1, msg_update, strlen(msg_update));
                write(partite[mio_id_partita].socket_g2, msg_update, strlen(msg_update));
                
                int vincitore = controlla_vittoria(mio_id_partita);
                if (vincitore != 0) {
                    partite[mio_id_partita].stato = TERMINATA; 
                    
                    if (vincitore == 3) {
                        write(partite[mio_id_partita].socket_g1, "PAREGGIO\n", 9);
                        write(partite[mio_id_partita].socket_g2, "PAREGGIO\n", 9);
                    } 
                    else if (vincitore == 1) {
                        write(partite[mio_id_partita].socket_g1, "VITTORIA\n", 9);
                        write(partite[mio_id_partita].socket_g2, "SCONFITTA\n", 10);
                    } 
                    else if (vincitore == 2) {
                        write(partite[mio_id_partita].socket_g1, "SCONFITTA\n", 10);
                        write(partite[mio_id_partita].socket_g2, "VITTORIA\n", 9);
                    }

                    char msg_notifica[100];
                    sprintf(msg_notifica, "NOTIFICA;La partita ID %d si e' conclusa.\n", mio_id_partita);
                    broadcast_notifica(mio_id_partita, msg_notifica);
                }
            } else {
                write(sock, "MOSSA_NON_VALIDA\n", 17);
            }
        }
        else if (strncmp(buffer, "REMATCH", 7) == 0) {
            pthread_mutex_lock(&partite[mio_id_partita].lock_partita);
            if (partite[mio_id_partita].stato == TERMINATA) {
                partite[mio_id_partita].voti_rematch++; 
                if (partite[mio_id_partita].voti_rematch == 2) {
                    pthread_mutex_unlock(&partite[mio_id_partita].lock_partita);
                    reset_griglia(mio_id_partita);
                    write(partite[mio_id_partita].socket_g1, "REMATCH_OK\n", 11);
                    write(partite[mio_id_partita].socket_g2, "REMATCH_OK\n", 11);

                    char msg_notifica[100];
                    sprintf(msg_notifica, "NOTIFICA;I giocatori della partita ID %d hanno accettato la rivincita! Il match ricomincia.\n", mio_id_partita);
                    broadcast_notifica(mio_id_partita, msg_notifica);
                } else {
                    pthread_mutex_unlock(&partite[mio_id_partita].lock_partita);
                }
            } else {
                pthread_mutex_unlock(&partite[mio_id_partita].lock_partita);
            }
        }
        else if (strncmp(buffer, "NO_REMATCH", 10) == 0) {
            int altro_socket = (mio_giocatore == 1) ? partite[mio_id_partita].socket_g2 : partite[mio_id_partita].socket_g1;
            write(altro_socket, "ABBANDONO\n", 10);
            
            pthread_mutex_lock(&partite[mio_id_partita].lock_partita);
            partite[mio_id_partita].stato = LIBERA; 
            pthread_mutex_unlock(&partite[mio_id_partita].lock_partita);

            char msg_notifica[100];
            sprintf(msg_notifica, "NOTIFICA;La stanza della partita ID %d e' stata chiusa ed e' di nuovo libera.\n", mio_id_partita);
            broadcast_notifica(mio_id_partita, msg_notifica);
        }
    }

    pthread_mutex_lock(&lock_globale);
    for (int i = 0; i < num_client_connessi; i++) {
        if (client_connessi[i] == sock) {
            for (int j = i; j < num_client_connessi - 1; j++) {
                client_connessi[j] = client_connessi[j + 1];
            }
            num_client_connessi--;
            break;
        }
    }
    pthread_mutex_unlock(&lock_globale);

    close(sock);
    return NULL;
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    inizializza_partite(); 

    // Creazione del socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Creazione socket fallita"); 
        exit(EXIT_FAILURE);
    }

    // 1. Abilita SO_REUSEADDR
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEADDR fallita"); 
        exit(EXIT_FAILURE);
    }

    // 2. Abilita SO_REUSEPORT (se supportato dal sistema)
    #ifdef SO_REUSEPORT
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) < 0) {
        perror("setsockopt SO_REUSEPORT fallita"); 
        exit(EXIT_FAILURE);
    }
    #endif

    // Configurazione indirizzo e porta
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    // Bind
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind fallito"); 
        exit(EXIT_FAILURE);
    }

    // Listen
    if (listen(server_fd, 10) < 0) {
        perror("Listen fallito"); 
        exit(EXIT_FAILURE);
    }

    printf("Server C Completato! In attesa sulla porta %d...\n", PORT);

    // Ciclo principale di accettazione
    while(1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("Accept fallito");
            continue;
        }

        // Registrazione nuovo client in mutua esclusione
        pthread_mutex_lock(&lock_globale);
        client_connessi[num_client_connessi++] = new_socket;
        pthread_mutex_unlock(&lock_globale);

        // Allocazione sicura del descrittore per il thread
        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;
        pthread_t sn_thread;
        
        if (pthread_create(&sn_thread, NULL, gestisci_client, (void*) new_sock) < 0) {
            perror("Errore thread");
            free(new_sock);
            continue;
        }
        pthread_detach(sn_thread);
    }
    
    close(server_fd);
    return 0;
}
#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#define RIGHE 6
#define COLONNE 7
#define MAX_PARTITE 10

typedef enum { LIBERA, IN_ATTESA, IN_ACCETTAZIONE, IN_CORSO, TERMINATA } StatoPartita;

typedef struct {
    int id_partita;
    int socket_g1;
    int socket_g2;
    int griglia[RIGHE][COLONNE];
    int turno_di;
    StatoPartita stato;
    int voti_rematch; 
    pthread_mutex_t lock_partita;
} Partita;

extern Partita partite[MAX_PARTITE];
extern pthread_mutex_t lock_globale;

void inizializza_partite();
int crea_partita(int socket_g1);
int unisciti_partita(int id_partita, int socket_g2);
int inserisci_gettone(int id_partita, int giocatore, int colonna);
int controlla_vittoria(int id_partita);
int gestisci_risposta_join(int id_partita, int accetta); 
void reset_griglia(int id_partita);

#endif
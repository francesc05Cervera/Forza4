#ifndef GAME_H
#define GAME_H

#include <pthread.h>

#define RIGHE 6 //definisco il numero di righe della griglia del gioco
#define COLONNE 7 //e il numero di colonne della griglia del gioco
#define MAX_PARTITE 10 // e le partite massime 

typedef enum { LIBERA, IN_ATTESA, IN_ACCETTAZIONE, IN_CORSO, TERMINATA } StatoPartita;
//enumerazione per i possibili stati della partita 

typedef struct { //struttura dati della partita
    int id_partita; //identificativo univoco della partita
    int socket_g1; //socket del primo giocatore
    int socket_g2; //socket del secondo giocatore
    int griglia[RIGHE][COLONNE]; //la griglia del gioco, rappresentata come una matrice di interi
    int turno_di; //indica a quale giocatore tocca fare la mossa (1 o 2)
    StatoPartita stato; //lo stato della partita (LIBERA, IN_ATTESA, IN_ACCETTAZIONE, IN_CORSO, TERMINATA)
    int voti_rematch; //voti per l'eventuale rivincita
    pthread_mutex_t lock_partita; //mutex per proteggere l'accesso alla struttura della partita da parte di più thread
} Partita;

extern Partita partite[MAX_PARTITE]; //array globale di partite disponibili sul server
extern pthread_mutex_t lock_globale; //mutex globale per proteggere l'accesso all'array delle partite e ad altre risorse condivise tra i thread


/* funzioni implementate in game.c */
void inizializza_partite();
int crea_partita(int socket_g1);
int unisciti_partita(int id_partita, int socket_g2);
int inserisci_gettone(int id_partita, int giocatore, int colonna);
int controlla_vittoria(int id_partita);
int gestisci_risposta_join(int id_partita, int accetta); 
void reset_griglia(int id_partita);

#endif
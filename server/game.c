#include "game.h"
#include <stdio.h>

Partita partite[MAX_PARTITE];
pthread_mutex_t lock_globale = PTHREAD_MUTEX_INITIALIZER;

void inizializza_partite() {
    for (int i = 0; i < MAX_PARTITE; i++) {
        partite[i].stato = LIBERA;
        partite[i].id_partita = i;
        pthread_mutex_init(&partite[i].lock_partita, NULL);
    }
}

int crea_partita(int socket_g1) {
    pthread_mutex_lock(&lock_globale);
    for (int i = 0; i < MAX_PARTITE; i++) {
        if (partite[i].stato == LIBERA) {
            partite[i].socket_g1 = socket_g1;
            partite[i].stato = IN_ATTESA;
            partite[i].turno_di = 1; 
            partite[i].voti_rematch = 0;
            for (int r = 0; r < RIGHE; r++)
                for (int c = 0; c < COLONNE; c++)
                    partite[i].griglia[r][c] = 0;
            pthread_mutex_unlock(&lock_globale);
            return i; 
        }
    }
    pthread_mutex_unlock(&lock_globale);
    return -1; 
}

int unisciti_partita(int id_partita, int socket_g2) {
    if (id_partita < 0 || id_partita >= MAX_PARTITE) return 0;
    
    pthread_mutex_lock(&partite[id_partita].lock_partita);
    if (partite[id_partita].stato == IN_ATTESA) {
        partite[id_partita].socket_g2 = socket_g2;
        partite[id_partita].stato = IN_ACCETTAZIONE; 
        pthread_mutex_unlock(&partite[id_partita].lock_partita);
        return 1;
    }
    pthread_mutex_unlock(&partite[id_partita].lock_partita);
    return 0; 
}

int gestisci_risposta_join(int id_partita, int accetta) {
    pthread_mutex_lock(&partite[id_partita].lock_partita);
    
    if (partite[id_partita].stato == IN_ACCETTAZIONE) {
        if (accetta) {
            partite[id_partita].stato = IN_CORSO; 
        } else {
            partite[id_partita].stato = IN_ATTESA; 
            partite[id_partita].socket_g2 = 0;    
        }
        pthread_mutex_unlock(&partite[id_partita].lock_partita);
        return 1;
    }
    
    pthread_mutex_unlock(&partite[id_partita].lock_partita);
    return 0;
}

int inserisci_gettone(int id_partita, int giocatore, int colonna) {
    if (colonna < 0 || colonna >= COLONNE) return -1;
    pthread_mutex_lock(&partite[id_partita].lock_partita);
    
    if (partite[id_partita].stato != IN_CORSO || partite[id_partita].turno_di != giocatore) {
        pthread_mutex_unlock(&partite[id_partita].lock_partita);
        return -1; 
    }
    for (int r = RIGHE - 1; r >= 0; r--) {
        if (partite[id_partita].griglia[r][colonna] == 0) {
            partite[id_partita].griglia[r][colonna] = giocatore;
            partite[id_partita].turno_di = (giocatore == 1) ? 2 : 1; 
            pthread_mutex_unlock(&partite[id_partita].lock_partita);
            return r; 
        }
    }
    pthread_mutex_unlock(&partite[id_partita].lock_partita);
    return -1; 
}

int controlla_vittoria(int id_partita) {
    int giocatore;
    for (int r = 0; r < RIGHE; r++) {
        for (int c = 0; c < COLONNE - 3; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 && partite[id_partita].griglia[r][c+1] == giocatore && 
                partite[id_partita].griglia[r][c+2] == giocatore && partite[id_partita].griglia[r][c+3] == giocatore) return giocatore;
        }
    }
    for (int r = 0; r < RIGHE - 3; r++) {
        for (int c = 0; c < COLONNE; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 && partite[id_partita].griglia[r+1][c] == giocatore && 
                partite[id_partita].griglia[r+2][c] == giocatore && partite[id_partita].griglia[r+3][c] == giocatore) return giocatore;
        }
    }
    for (int r = 3; r < RIGHE; r++) {
        for (int c = 0; c < COLONNE - 3; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 && partite[id_partita].griglia[r-1][c+1] == giocatore && 
                partite[id_partita].griglia[r-2][c+2] == giocatore && partite[id_partita].griglia[r-3][c+3] == giocatore) return giocatore;
        }
    }
    for (int r = 0; r < RIGHE - 3; r++) {
        for (int c = 0; c < COLONNE - 3; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 && partite[id_partita].griglia[r+1][c+1] == giocatore && 
                partite[id_partita].griglia[r+2][c+2] == giocatore && partite[id_partita].griglia[r+3][c+3] == giocatore) return giocatore;
        }
    }

    int celle_piene = 0;
    for (int c = 0; c < COLONNE; c++) {
        if (partite[id_partita].griglia[0][c] != 0) {
            celle_piene++;
        }
    }
    if (celle_piene == COLONNE) return 3; 

    return 0; 
}

void reset_griglia(int id_partita) {
    pthread_mutex_lock(&partite[id_partita].lock_partita);
    for (int r = 0; r < RIGHE; r++) {
        for (int c = 0; c < COLONNE; c++) {
            partite[id_partita].griglia[r][c] = 0;
        }
    }
    partite[id_partita].turno_di = 1; 
    partite[id_partita].stato = IN_CORSO;
    partite[id_partita].voti_rematch = 0;
    pthread_mutex_unlock(&partite[id_partita].lock_partita);
}
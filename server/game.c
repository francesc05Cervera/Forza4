#include "game.h" //includo il file header
#include <stdio.h> //e la libreria di standard input/output di C

Partita partite[MAX_PARTITE]; //array globale di partite disponibili sul server
pthread_mutex_t lock_globale = PTHREAD_MUTEX_INITIALIZER; //inizializzo il lock globale

void inizializza_partite() { //funzione per inizializzare le partite disponibili sul server
    for (int i = 0; i < MAX_PARTITE; i++) //per ongni partita disponibile
    {
        partite[i].stato = LIBERA; //lo stato è libero
        partite[i].id_partita = i; // gli assegno come ID l'indice dell'array
        pthread_mutex_init(&partite[i].lock_partita, NULL); //inizializzo il mutex 
    }
}

int crea_partita(int socket_g1) //funzione per creare la partita, la crea il g1 (socket_g1) 
{
    pthread_mutex_lock(&lock_globale); //blocco il mutex globale per proteggere l'accesso all'array delle partite
    for (int i = 0; i < MAX_PARTITE; i++) 
    {
        if (partite[i].stato == LIBERA) //mi cerco la partita libera
        {
            /* trovata partita libera */
            partite[i].socket_g1 = socket_g1; //assegno il socket del giocatore 1 alla partita
            partite[i].stato = IN_ATTESA; //imposto lo stato della partita a IN_ATTESA per indicare che il giocatore 1 ha creato la partita e sta aspettando un giocatore 2
            partite[i].turno_di = 1; //imposto il turno di gioco al giocatore 1
            partite[i].voti_rematch = 0; //imposto i voti per la rivincita a 0
            for (int r = 0; r < RIGHE; r++) //azzero la griglia della partita
                for (int c = 0; c < COLONNE; c++) 
                    partite[i].griglia[r][c] = 0;
            pthread_mutex_unlock(&lock_globale); //sblocco il mutex globale dopo aver modificato lo stato della partita
            return i; //restituisco l'id partita appena creata
        }
    }
    pthread_mutex_unlock(&lock_globale); //in ogni caso se non entra nel if sblocco cmq il mutex
    return -1; // ma ritorno -1 come segno di errore 
}

int unisciti_partita(int id_partita, int socket_g2)
{ //funzione per far unire un giocatore 2 a una partita esistente, riceve in input l'id della partita e il socket del giocatore 2
    if (id_partita < 0 || id_partita >= MAX_PARTITE) return 0; //verifica che l'id della partita sia valido, se non lo è ritorna 0 come segno di errore
    
    //se lo è...
    pthread_mutex_lock(&partite[id_partita].lock_partita);  //blocco il mutex della partita
    if (partite[id_partita].stato == IN_ATTESA) //verifico che realmente sta in attesa di giocatori 
    {
        partite[id_partita].socket_g2 = socket_g2; //gli assegno il socket del g2
        partite[id_partita].stato = IN_ACCETTAZIONE; //aggiorno stato a IN_ACCETTAZIONE 
        pthread_mutex_unlock(&partite[id_partita].lock_partita); //sblocco mutex
        return 1;
    }
    pthread_mutex_unlock(&partite[id_partita].lock_partita); //in ogni caso sblocco il mutex della partita
    return 0; 
}

int gestisci_risposta_join(int id_partita, int accetta)
{ //funzione per gestire la risposta di un join
    pthread_mutex_lock(&partite[id_partita].lock_partita); //blocco il mutex 
    
    if (partite[id_partita].stato == IN_ACCETTAZIONE) //verifico che ralmente sia in accettazione 
    {
        if (accetta) { //se accetta = 1 (TRUE) allora il giocatore 1 ha accettato la richiesta del giocatore 2
            partite[id_partita].stato = IN_CORSO;  //e la partita diventa in corso
        } else { //altrimenti...
            partite[id_partita].stato = IN_ATTESA; //la partita torna in attesa di un altro giocatore 2
            partite[id_partita].socket_g2 = 0;    //e il socket del giocatore 2 viene azzerato
        }
        pthread_mutex_unlock(&partite[id_partita].lock_partita); //sblocco il mutex della partita
        return 1;
    }
    
    pthread_mutex_unlock(&partite[id_partita].lock_partita); //in ogni caso l'avrei sbloccato
    return 0;
}

int inserisci_gettone(int id_partita, int giocatore, int colonna) 
{ //funzione per inserire un gettone nella griglia della partita, riceve in input l'id della partita, il numero del giocatore (1 o 2) e la colonna in cui inserire il gettone
    if (colonna < 0 || colonna >= COLONNE) return -1; //controlla che tutto sia valido
    pthread_mutex_lock(&partite[id_partita].lock_partita); //se si, blocco mutex
    
    if (partite[id_partita].stato != IN_CORSO || partite[id_partita].turno_di != giocatore) 
    { //faccio tutti i controlli sullo stato della partita e sul turno di gioco 
        pthread_mutex_unlock(&partite[id_partita].lock_partita); //sblocco mutex
        return -1; 
    }

    /* Inserisco il gettone nella prima riga disponibile */
    for (int r = RIGHE - 1; r >= 0; r--) {
        if (partite[id_partita].griglia[r][colonna] == 0) {
            partite[id_partita].griglia[r][colonna] = giocatore;
            partite[id_partita].turno_di = (giocatore == 1) ? 2 : 1; 
            pthread_mutex_unlock(&partite[id_partita].lock_partita); //dopo aver inserito il gettone, sblocco il mutex della partita
            return r; 
        }
    }
    pthread_mutex_unlock(&partite[id_partita].lock_partita); //lo sbloccherei in ogni caso qui
    return -1; 
}

/* logica di controllo vittoria */
int controlla_vittoria(int id_partita) 
{
    int giocatore;

    /* Orizzontale */
    for (int r = 0; r < RIGHE; r++) {
        for (int c = 0; c < COLONNE - 3; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 &&
                partite[id_partita].griglia[r][c+1] == giocatore &&
                partite[id_partita].griglia[r][c+2] == giocatore &&
                partite[id_partita].griglia[r][c+3] == giocatore) return giocatore;
        }
    }

    /* Verticale */
    for (int r = 0; r < RIGHE - 3; r++) {
        for (int c = 0; c < COLONNE; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 &&
                partite[id_partita].griglia[r+1][c] == giocatore &&
                partite[id_partita].griglia[r+2][c] == giocatore &&
                partite[id_partita].griglia[r+3][c] == giocatore) return giocatore;
        }
    }

    /* Diagonale ascendente */
    for (int r = 3; r < RIGHE; r++) {
        for (int c = 0; c < COLONNE - 3; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 &&
                partite[id_partita].griglia[r-1][c+1] == giocatore &&
                partite[id_partita].griglia[r-2][c+2] == giocatore &&
                partite[id_partita].griglia[r-3][c+3] == giocatore) return giocatore;
        }
    }

    /* Diagonale discendente */
    for (int r = 0; r < RIGHE - 3; r++) {
        for (int c = 0; c < COLONNE - 3; c++) {
            giocatore = partite[id_partita].griglia[r][c];
            if (giocatore != 0 &&
                partite[id_partita].griglia[r+1][c+1] == giocatore &&
                partite[id_partita].griglia[r+2][c+2] == giocatore &&
                partite[id_partita].griglia[r+3][c+3] == giocatore) return giocatore;
        }
    }

    /* FIX: pareggio — la riga 0 completamente piena significa griglia piena in Forza 4 */
    int pareggio = 1;
    for (int c = 0; c < COLONNE; c++) {
        if (partite[id_partita].griglia[0][c] == 0) {
            pareggio = 0;
            break;
        }
    }
    if (pareggio) return 3;

    return 0;
}

/* funzione per resettare la griglia della partita */
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
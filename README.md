#Progetto Forza4 di Cervera Francesco e Correra Salvatore

## Requisiti
1. Docker Desktop installato e in esecuzione in background
2. JDK Installato per eseguire le GUI in Java Swing 
3. Ambiente di esecuzione Java (ex. Eclipse)

## Come Avviare il Server

Aprire la cartella del progetto con VS Code in WSL, e eseguire il comando: 
docker compose up --build 

appena comparirà la scritta "Server C Completato! In attesa sulla porta 9090..." 
vuol dire che il server è in ascolto sulla porta 9090 ed è pronto a ricevere richieste
dai client 

## Come Avviare il Client 

Dall'ambiente di esecuzione Java è sufficiente cliccare il tasto "Run" per
avviare la classe Forza4Menu dalla quale è possibile avviare una nuova partita
o unirsi a una partita

## Come Chiudere il Client 

E' sufficiente premere il tasto "X" di Windows

## Come arrestare il server 

CTRL+C oppure con il comando docker compose down

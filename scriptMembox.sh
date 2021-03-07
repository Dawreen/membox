#!/bin/bash

FSTAMP="1"
NOME="timestamp"
#Valuto Opzioni
while  getopts "p:u:g:r:c:s:o:m:h:" opt; do
	case $opt in
		p)
		NOME="$NOME PUT PUTFAILED"
		FSTAMP="$FSTAMP,3,4"
    ;;
		u)
		NOME="$NOME UPDATE UPDATEFAILED"
		FSTAMP="$FSTAMP,5,6"
    ;;
    g)
		NOME="$NOME GET GETFAILED"
		FSTAMP="$FSTAMP,7,8"
    ;;
    r)
		NOME="$NOME REMOVE REMOVEFAILED"
		FSTAMP="$FSTAMP,9,10"
    ;;
    c)
		NOME="$NOME CONNECTIONS"
		FSTAMP="$FSTAMP,13"
    ;;
		s)
		NOME="$NOME SIZE"
		FSTAMP="$FSTAMP,14"
		;;
    o)
		NOME="$NOME OBJECTS"
		FSTAMP="$FSTAMP,16"
    ;;
    m)
		NOME="$NOME MAXSIZE MAXOBJECTS"
		FSTAMP="$FSTAMP,15,17"
    ;;
    h)
      echo "
       Senza nessuna opzione lo script stampa tutte le statistiche
       dell’ultimo timestamp. L’opzione -p stampa il numero di PUT OP
       ed il numero di PUT OP fallite per tutti i timestamp. L’opzione
       -u stampa il numero di UPDATE OP ed il numero di UP-DATE OP
       fallite per tutti i timestamp. L’opzione -g stampa il numero
       di GET OP ed il numero di GET OP fallite per tutti i timestamp.
       L’opzione -r stampa il numero di REMOVE OP ed il numero di
       REMOVE OP fallite per tutti i timestamp. L’opzione -c stampa il
       numero di connessioni per tutti i timestamp. L’opzione -s stampa
       la size in KB per tutti i timestamp. L’opzione -o stampa il numero
       di oggetti nel repository per tutti i timestamp. L’opzione -m
       stampa il massimo numero di connessioni raggiunte dal server, il
       massimo numero di oggetti memorizzati ed la massima size in KB
       raggiunta. Infine l’opzione lunga help stampa un messaggio di uso
       dello script.
       "
    ;;
  esac
done
(echo "$NOME"
cut -d" " -f$FSTAMP "${BASH_ARGV[0]}") | column -t

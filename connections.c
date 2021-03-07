#ifndef CONNECTIONS_H_
#define CONNECTIONS_H_

#define MAX_RETRIES     10
#define MAX_SLEEPING     3
#if !defined(UNIX_PATH_MAX)
#define UNIX_PATH_MAX  64
#endif

#include "message.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>

/**
 * @file  connection.h
 * @brief Contiene le funzioni che implementano il protocollo
 *        tra i clients ed il server membox
 */

/**
 * @function openConnection
 * @brief Apre una connessione AF_UNIX verso il server membox.
 *
 * @param path Path del socket AF_UNIX
 * @param ntimes numero massimo di tentativi di retry
 * @param secs tempo di attesa tra due retry consecutive
 *
 * @return il descrittore associato alla connessione in caso di successo
 *         -1 in caso di errore
 */
int openConnection(char* path, unsigned int ntimes, unsigned int secs){
  int fd, ntent = 0, aux = 1;
  struct sockaddr_un sa;
  strncpy(sa.sun_path, path, UNIX_PATH_MAX);
  sa.sun_family = AF_UNIX;
  unlink("path");
  if( (fd = socket( AF_UNIX, SOCK_STREAM, 0)) == -1){
    fprintf(stderr, "errore nella creazione del socket\n");
    return -1;
  }
  while( ntent < ntimes && (aux = connect(fd, (struct sockaddr*)&sa, sizeof(sa))) == -1 ){
    if( errno == ENOENT ){
      ntent+=1;
      sleep(secs);
    }else{
      fprintf(stderr, "errore in connection\n");
      return -1;
    }
  }
  return aux == 0 ? fd : -1;
}

// -------- server side -----

/**
 * @function readHeader
 * @brief Legge l'header del messaggio
 *
 * @param fd     descrittore della connessione
 * @param hdr    puntatore all'header del messaggio da ricevere
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int readHeader(long fd, message_hdr_t *hdr){
  int sum = 0, s;

  while( (s = read(fd, ((char*)hdr)+sum, sizeof(message_hdr_t)-sum)) > 0 && sum != sizeof(message_hdr_t)){
    sum += s;
  }
  if( s == 0 && sum != sizeof(message_hdr_t))
    return -1;
  if ( s < 0 )
    return -1;
  return 0;
}

/**
 * @function readData
 * @brief Legge il body del messaggio
 *
 * @param fd     descrittore della connessione
 * @param data   puntatore al body del messaggio
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int readData(long fd, message_data_t *data){
  unsigned int len = 0, sum = 0;
  int s;
  while( (s = read(fd, ((char*)&(data->len))+sum, sizeof(data->len)-sum)) > 0 && sum != sizeof(data->len)){
    sum += s;
  }
  if( s == 0 && sum != sizeof(data->len))
    return -1;
  if ( s < 0 )
    return -1;

  sum = 0;
  len = data->len;
  data->buf = (char*)malloc((data->len)*sizeof(char));
  while( (s = read(fd, ((char*)(data->buf))+sum, (len*sizeof(char)-sum))) > 0 && sum != sizeof(char)*len) {
    sum += s;
  }

  if( s == 0 && sum != sizeof(char)*len)
    return -1;
  if ( s < 0 )
    return -1;
  return 0;
}

/* da completare da parte dello studente con altri metodi di interfaccia */
int writeHeader( long fd, message_hdr_t *hdr){
  int sum = 0, s;
  errno = 0;
  while( (s = write(fd, ((char*)hdr)+sum, sizeof(message_hdr_t)-sum)) >= 0 && sum != sizeof(message_hdr_t) ){
    sum += s;
  }
  if( s < 0){
    return -1;
  }
  return 0;
}

int writeData( long fd, message_data_t *data){
  unsigned int len = data->len;
  long unsigned int sum = 0;
  long int s;
  while( (s = write(fd, ((char*)&(data->len))+sum, sizeof(data->len)-sum)) >= 0 && sum != sizeof(data->len)){
    sum += s;
  }
  if( s < 0) return -1;
  sum = 0;
  while( (s = write(fd, ((char*)(data->buf))+sum, (len*sizeof(char)-sum))) >= 0 && sum != len*sizeof(char)) {
    sum += s;
  }
  if( s < 0) return -1;
  return 0;
}



// ------- client side ------
/**
 * @function sendRequest
 * @brief Invia un messaggio di richiesta al server membox
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da inviare
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
 int sendRequest( long fd, message_t *msg){
   if( writeHeader(fd, &(msg->hdr)) != 0){
     return -1;}
   if( msg->data.buf != NULL && writeData(fd, &(msg->data)) != 0){
     return -1;}
   return 0;
}


/**
 * @function readReply
 * @brief Legge un messaggio di risposta dal server membox
 *
 * @param fd     descrittore della connessione
 * @param msg    puntatore al messaggio da ricevere
 *
 * @return 0 in caso di successo -1 in caso di errore
 */
int readReply(long fd, message_t *msg){
  if( readHeader(fd, &(msg->hdr)) != 0) return -1;
  if( readData(fd, &(msg->data)) != 0) return -1;
  return 0;
}

#endif /* CONNECTIONS_H_ */

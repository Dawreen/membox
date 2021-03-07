#define _POSIX_C_SOURCE 200809L
#include <stdio.h>

#include "parser.h"
#define N 256

/**
  Recupera il valore associato dalla riga passata.
  |param str Stringa con formattazione nome = valore
  |retval Una stringa contentente il valore della configurazione analizzata
          NULL se dopo l'uguale non c'è nulla oppure un commento
*/
char *estrattore( char *str){
  int i = 0, j = 0;
  while( str[i] != '=' ){
    if(str[i] == '\0') return NULL;
    i++;
  }
  i++;
  while( isspace(str[i]) ){
    if( str[i] == '\0' || str[i] == '#') return NULL;
    i++;
  }
  j = i;
  while( str[j] != '\0' && str[j] != '#')
    j++;
  while( isspace(str[j]) )
    j--;
  str[j-1] = '\0';

	return str+i;
}

/**
  Assegna le configurazione parsando il file path
  \param path Nome del file di configuazione
  \cf Struttura dati dove salvare le configurazioni

  \retval 0 se va tutto bene
  \retval 1 se si è verificato un errore
*/
int assignConf(char *path, param *cf){
  FILE *fd;
  char *buf = NULL;
  char *tmp = NULL;

  int er = 0;
  size_t n = 0;

  if((fd = fopen(path, "r")) == NULL) return -1;
  while( (er = getline(&buf, &n, fd)) > 0){

	if( (buf[0] != '#') && (buf[0] != ' ' && (buf[0] != '\n'))){
      if(strncmp(buf, "UnixPath", sizeof("UnixPath")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) strcpy((*cf).UnixPath, tmp);
      }

      if (strncmp(buf, "MaxConnections", sizeof("MaxConnections")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) (*cf).MaxConnections = strtol(tmp, NULL, 10);
      }

      if (strncmp(buf, "ThreadsInPool", sizeof("ThreadsInPool")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) (*cf).ThreadsInPool = strtol(tmp, NULL, 10);
      }

      if (strncmp(buf, "StorageSize", sizeof("StorageSize")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) (*cf).StorageSize = strtol(tmp,NULL,10);
      }

      if (strncmp(buf, "StorageByteSize", sizeof("StorageByteSize")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) (*cf).StorageByteSize = strtol(tmp,NULL,10);
      }

      if (strncmp(buf, "MaxObjSize", sizeof("MaxObjSize")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) (*cf).MaxObjSize = strtol(tmp,NULL,10);
      }

      if (strncmp(buf, "StatFileName", sizeof("StatFileName")-1) == 0){
        tmp = estrattore(buf);
        if( tmp != NULL) strcpy((*cf).StatFileName, tmp);
      }
    }
  }
  free(buf);
  if(fclose( fd) != 0) return -1;
  return 0;
}

/*
int main(int argc, char *argv[]){
	int option;
	param da;

//	Per il parsing delle opzioni usiamo getopt, decisamente più elegante. Vi torna comodo anche per il bash
	while ( (option = getopt(argc, argv,"f:")) != -1) {
		switch (option) {
			case 'f' :{
				if(assignConf(optarg, &da))
					perror("Errore assignConf\n");
				break;
			}
			default:{
				perror("Errore nel parsing delle opzoni");
				break;
			}
		}
	}

  printf("UnixPath = %s;\nMaxConnections = %d;\nThreadsInPool = %d;\nStorageSize = %d;\nStorageByteSize = %d;\nMaxObjSize = %d;\nStatFileName = %s;\n",
      da.UnixPath,
      da.MaxConnections,
      da.ThreadsInPool,
      da.StorageSize,
      da.StorageByteSize,
      da.MaxObjSize,
      da.StatFileName);
  return 0;
}*/

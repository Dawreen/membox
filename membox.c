/*
 * membox Progetto del corso di LSO 2016
 *
 * Dipartimento di Informatica Università di Pisa
 * Docenti: Pelagatti, Torquati
 *
 */
/**
 * @file membox.c
 * @brief File principale del server membox
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>

#include "icl_hash.h"
#include "message.h"
#include "parser.c"
#include "connections.h"
#include "threadpool.h"
#include "intList.h"

#include "stats.h"
int close_all;
int stopAccept;
struct sigaction sa;

/* struttura che memorizza le statistiche del server, struct statistics
 * e' definita in stats.h.
 *
 */


struct statistics  mboxStats = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
pthread_mutex_t lockStats = PTHREAD_MUTEX_INITIALIZER;

/*--- strutture globali ---*/
static icl_hash_t *hashTable;
static threadpool_t *pool;
pthread_rwlock_t mTable = PTHREAD_RWLOCK_INITIALIZER;
pthread_mutex_t lockSt = PTHREAD_MUTEX_INITIALIZER;
static int statoLock = -1;
static param *da;

static list_t *fd_list;
pthread_mutex_t lockList = PTHREAD_MUTEX_INITIALIZER;

/*--- funzioni ausiliarie ---*/
void freeData(void* data){
  free(((message_data_t*)data)->buf);
  free(data);
}
/*--- fine funzioni ausiliarie ---*/

/*--- implementazione operazioni ---*/
/**
 * @function put_op
 * @brief Inserisce un elemento nella hashTable
 *
 * @param key chiave dell'oggetto da inserire
 * @param data oggetto da inserire nella hashTable
 *
 * @return 0 in caso di successo
 * @return -1 operazione fallita
 * @return -2 oggetto già presente nella hashTable
 */
int put_op( membox_key_t key, message_data_t *data){
  pthread_rwlock_wrlock(&mTable);
  if( icl_hash_find( hashTable, (void*)key) != NULL){
    pthread_rwlock_unlock( &mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nput_failed += 1;
    mboxStats.nput += 1;
    pthread_mutex_unlock(&lockStats);

    return -2;
  }

  message_data_t* data_copy = (message_data_t*)malloc(sizeof(message_data_t));
  if (data_copy == NULL) {
      return -1;
  }
  memcpy(data_copy, data, sizeof(message_data_t));

  if( icl_hash_insert( hashTable, (void*)key, (void*)data_copy) == NULL){
    pthread_rwlock_unlock( &mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nput_failed += 1;
    mboxStats.nput += 1;
    pthread_mutex_unlock(&lockStats);

    return -1;
  }
  pthread_rwlock_unlock( &mTable);

  pthread_mutex_lock(&lockStats);
  mboxStats.nput += 1;
  pthread_mutex_unlock(&lockStats);

  return 0;
}

/**
 * @function update_op
 * @brief aggiorna un elemento già presente nella hashTable
 *
 * @param key chiave del oggetto da aggiornare
 * @param data oggetto da aggiornare nella hashTable
 *
 * @return 0 in caso di successo
 * @return -1 opreazione fallita
 * @return -2 oggetto non presente nella hashTable
 * @return -3 size dell'oggetto da aggiornare non corrisponde
 */
int update_op( membox_key_t key, message_data_t *data){
  pthread_rwlock_wrlock( &mTable);
  message_data_t *olddata = (message_data_t*)icl_hash_find( hashTable, (void*)key);
  if( olddata == NULL){
    pthread_rwlock_unlock(&mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nupdate_failed += 1;
    mboxStats.nupdate += 1;
    pthread_mutex_unlock(&lockStats);
    return -2;
  }else{
    if( data->len != olddata->len){
      pthread_rwlock_unlock(&mTable);

      pthread_mutex_lock(&lockStats);
      mboxStats.nupdate_failed += 1;
      mboxStats.nupdate += 1;
      pthread_mutex_unlock(&lockStats);

      return -3;
    }

    message_data_t* data_copy = (message_data_t*)malloc(sizeof(message_data_t));
    if (data_copy == NULL) {
        return -1;
    }
    memcpy(data_copy, data, sizeof(message_data_t));

    if( icl_hash_update_insert( hashTable, (void*)key, (void*)data_copy, NULL) == NULL){
      pthread_rwlock_unlock( &mTable);

      pthread_mutex_lock(&lockStats);
      mboxStats.nupdate_failed += 1;
      mboxStats.nupdate += 1;
      pthread_mutex_unlock(&lockStats);
      return -1;
    }

    freeData(olddata);
    pthread_rwlock_unlock( &mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nupdate += 1;
    pthread_mutex_unlock(&lockStats);
    return 0;
  }
}

/**
 * @function get_op
 * @brief trova un elemento nello storage
 *
 * @param key chiave dell'oggetto da trovare
 * @param data puntatore dove inserire l'oggetto
 *
 * @return 0 in caso di successo
 * @return -1 opreazione fallita
 * @return -2 oggetto non presente nella storage
 */
int get_op( membox_key_t key, message_data_t *data){
  void* aux = NULL;
  pthread_rwlock_rdlock(&mTable);
  if((aux = icl_hash_find( hashTable, (void*)key)) == NULL){
    pthread_rwlock_unlock(&mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nget_failed += 1;
    mboxStats.nget += 1;
    pthread_mutex_unlock(&lockStats);
    return -2;
  }else{
    memcpy(data, aux, sizeof(message_data_t));
    pthread_rwlock_unlock(&mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nget += 1;
    pthread_mutex_unlock(&lockStats);
    return 0;
  }
}

/**
 * @function remove_op
 * @brief elimina un elemento dallo storage
 *
 * @param key chiave dell'oggetto da eliminare
 *
 * @return 0 in caso di successo
 * @return -1 opreazione fallita
 * @return -2 oggetto non presente nella storage
 */
int remove_op( membox_key_t key, long unsigned int *dim){
  void* aux;
  pthread_rwlock_wrlock(&mTable);
  if((aux = icl_hash_find( hashTable, (void*)key)) == NULL){
    pthread_rwlock_unlock( &mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nremove += 1;
    mboxStats.nremove_failed += 1;
    pthread_mutex_unlock(&lockStats);
    return -2;
  }
  *dim = ((message_data_t*)aux)->len;
  if(icl_hash_delete( hashTable, (void*)key, NULL, freeData) != 0){
    pthread_rwlock_unlock( &mTable);

    pthread_mutex_lock(&lockStats);
    mboxStats.nremove += 1;
    mboxStats.nremove_failed += 1;
    pthread_mutex_unlock(&lockStats);
    return -1;
  }
  pthread_rwlock_unlock( &mTable);

  pthread_mutex_lock(&lockStats);
  mboxStats.nremove += 1;
  pthread_mutex_unlock(&lockStats);
  return 0;
}
/*--- fine funzioni ---*/

/*--- worker ---*/
void* worker( void* arg){
  int flagGet = 0;
  int flagLock = 0;
  int res = 0;

  long fd = (long)arg;
  message_t op_to_do = { .hdr = { .op = 6, .key = 0}, .data = { .len = 0, .buf = NULL}};
  message_t reply = { .hdr = { .op = 6, .key = 0}, .data = { .len = 0, .buf = NULL}};

  while((res = readHeader( fd, &(op_to_do.hdr))) == 0){
    flagGet = 0;
    message_hdr_t hdr = op_to_do.hdr;
    if( statoLock == -1 || fd == statoLock){
      switch (hdr.op) {
        case 0: //PUT_OP
          {
            if(readData( fd, &(op_to_do.data)) != 0){
              setHeader(&reply, 12, &(hdr.key));
            }else{
              pthread_mutex_lock(&lockStats);
              if( da->StorageSize != 0 && (mboxStats.current_objects + 1) > da->StorageSize){
                setHeader(&reply, 14, &(hdr.key));
                pthread_mutex_unlock(&lockStats);
              }else{
                  if( da->StorageByteSize != 0 && (mboxStats.current_size + op_to_do.data.len) > da->StorageByteSize){
                    setHeader(&reply, 16, &(hdr.key));
                    pthread_mutex_unlock(&lockStats);
                  }else{
                    pthread_mutex_unlock(&lockStats);

                    if( da->MaxObjSize != 0 && op_to_do.data.len > da->MaxObjSize)
                      setHeader(&reply, 15, &(hdr.key));
                    else{
                      switch (put_op( hdr.key, &op_to_do.data)) {
                        case 0:
                          {
                            pthread_mutex_lock(&lockStats);
                            setHeader(&reply, 11, &(hdr.key));
                            mboxStats.current_objects++;
                            if(mboxStats.max_objects < mboxStats.current_objects)
                              mboxStats.max_objects = mboxStats.current_objects;
                            mboxStats.current_size+=op_to_do.data.len;
                            if(mboxStats.max_size < mboxStats.current_size)
                              mboxStats.max_size = mboxStats.current_size;
                            pthread_mutex_unlock(&lockStats);
                          }
                          break;
                        case -1:
                          {
                            setHeader(&reply, 12, &(hdr.key));
                          }
                          break;
                        case -2:
                          {
                            setHeader(&reply, 13, &(hdr.key));
                          }
                      }
                    }
                  }
                }
              }
            }
          break;
        case 1: //UPDATE_OP
          {
            if(readData( fd, &(op_to_do.data)) != 0){
              setHeader(&reply, 12, &(hdr.key));
            }else{
              //message_data_t data = op_to_do.data;
              switch (update_op( hdr.key, &(op_to_do.data))) {
                case 0:
                  {
                    setHeader( &reply, 11 ,&(hdr.key));
                  }
                break;
                case -1: setHeader( &reply, 12, &(hdr.key));
                break;
                case -2: setHeader( &reply, 20, &(hdr.key));
                break;
                case -3: setHeader( &reply, 19, &(hdr.key));
              }
            }
          }
          break;
        case 2: //GET_OP
          {
            switch (get_op( hdr.key, &(reply.data))) {
              case 0:
                {
                  setHeader(&reply, 11, &(hdr.key));
                  flagGet = 1;
                }
              break;
              case -1: setHeader(&reply, 12, &(hdr.key));
              break;
              case -2: setHeader(&reply, 17, &(hdr.key));
              break;
            }
          }
          break;
        case 3: //REMOVE_OP
          {
              long unsigned int dim = 0;
              switch (remove_op( hdr.key, &dim)) {
              case 0:
                {
                  pthread_mutex_lock(&lockStats);
                  mboxStats.current_objects--;
                  mboxStats.current_size-=dim;
                  pthread_mutex_unlock(&lockStats);
                  setHeader(&reply, 11, &(hdr.key));
                }
              break;
              case -1: setHeader(&reply, 12, &(hdr.key));
              break;
              case -2: setHeader(&reply, 18,&(hdr.key));
              break;
            }
          }
          break;
        case 4: //LOCK_OP
          {
            if( fd == statoLock){
              pthread_mutex_lock(&lockStats);
              mboxStats.nlock_failed +=1;
              mboxStats.nlock +=1;
              pthread_mutex_unlock(&lockStats);

              setHeader(&reply, 21, &(hdr.key));
            }else{
              pthread_mutex_lock(&lockSt);
              flagLock = 1;
              statoLock = fd;
              pthread_mutex_unlock(&lockSt);

              pthread_mutex_lock(&lockStats);
              mboxStats.nlock +=1;
              pthread_mutex_unlock(&lockStats);

              setHeader(&reply, 11, &(hdr.key));
            }
          }
          break;
        case 5: //UNLOCK_OP
          {
            if( statoLock == -1){
              setHeader(&reply, 22, &(hdr.key));
            }else{
              pthread_mutex_unlock(&lockSt);
              flagLock = 0;
              statoLock = -1;
              pthread_mutex_unlock(&lockSt);
              setHeader(&reply, 11, &(hdr.key));
            }
            if( sendRequest(fd, &reply) < 0){
              fprintf(stderr, "errore risposta server\n");
            }
          }
        default: setHeader(&reply, 12, &(hdr.key));
      }
    }else {
      if (hdr.op == PUT_OP || hdr.op == UPDATE_OP) {
        readData(fd, &(op_to_do.data));
      }
      setHeader(&reply, 21, &(hdr.key));
    }
    if( writeHeader(fd, &(reply.hdr)) != 0){
      fprintf(stderr, "errore risposta %d\n", errno);
      break;
    }
    if( flagGet == 1){
      if( writeData(fd, &(reply.data)) != 0){
        fprintf(stderr, "errore write data\n");
        break;
      }
    }

  }
  if(flagLock == 1){
    pthread_mutex_lock(&lockSt);
    statoLock = -1;
    pthread_mutex_unlock(&lockSt);
  }

  pthread_mutex_lock(&lockList);
  deleteList(&fd_list, fd);
  pthread_mutex_unlock(&lockList);
  close(fd);
  return NULL;
}

param *default_conf(){
  param *da = (param*)malloc(sizeof(param));
  strcpy(da->UnixPath, "/tmp/SocketFile");
  da->MaxConnections = 32;
  da->ThreadsInPool = 8;
  da->StorageSize = 0;
  da->StorageByteSize = 0;
  da->MaxObjSize = 0;
  strcpy(da->StatFileName, "/tmp/StatFile.txt");
  return da;
}

int compare( void* a, void* b){
  return ((int*)&a) - ((int*)&b);
}

unsigned int long_hash(void* x) { return (unsigned int)(long)x; }

/*
    Questa funzione appende al file delle statistiche i numeri inseriti nella struttura dichiarati nel file stats.h
    e viene invocata dal gestore_segnali (funzione)
*/
void appendStatToFile( char* st){
    mboxStats.concurrent_connections = lenList(&fd_list);
    FILE* file = fopen( st, "a");
    printStats(file);
    fclose(file);
}

/*
    Funzione di gestione dei segnali. Viene avviata alla ricezione di un segnale
*/
typedef struct argSegnali_s{
  char* st;
  int fd;
}arg_segnali_t;

static void* gestore_segnali(void* arg){
	int sig_arrived;
  arg_segnali_t *aS = (arg_segnali_t*)arg;


	while(!close_all){
		sigwait(&sa.sa_mask, &sig_arrived);
		if(sig_arrived == SIGINT || sig_arrived == SIGTERM || sig_arrived == SIGQUIT){
			close_all=1;
      if( shutdown( aS->fd, SHUT_RDWR) != 0){
        perror("errore shudown accept");
      }
      freePool(pool, 0);
			return NULL;
		}
		else if(sig_arrived == SIGUSR1){
			appendStatToFile( aS->st);
		}
		else if(sig_arrived == SIGUSR2){
      close_all = 1;
      if( shutdown( aS->fd, SHUT_RDWR) != 0){
        perror("errore shudown accept");
      }
      freePool( pool, 1);
			return NULL;
    }
	}
	return NULL;
}

int main(int argc, char *argv[]) {
  fd_list = NULL;
  stopAccept = 0;
  close_all = 0;
  arg_segnali_t *aS = (arg_segnali_t*)calloc(1, sizeof(arg_segnali_t));

  long option, skt_id, skt_id1;

  da = default_conf();
  /*acquisizione del file di configurazione*/
  while ( (option = getopt(argc, argv,"f:")) != -1) {
    switch (option) {
      case 'f' :{
        if(assignConf(optarg, da))
          perror("Errore assignConf\n");
        break;
      }
      default:{
        perror("Errore nel parsing delle opzoni");
        break;
      }
    }
  }

  skt_id = socket( AF_UNIX, SOCK_STREAM, 0);
  struct sockaddr_un sa1;
  strncpy( sa1.sun_path, da->UnixPath, UNIX_PATH_MAX);
  sa1.sun_family = AF_UNIX;
  unlink(da->UnixPath);
  bind( skt_id, (struct sockaddr *)&sa1, sizeof(sa1));
  listen(skt_id, 10);

  /*
      Avvio la gestione dei segnali. Maschero subito dopo tutti i segnali così che solo il gestore sia in grado
      di gestirli
  */
  sa.sa_handler=SIG_IGN;
  sigemptyset(&sa.sa_mask);
  sigaddset(&sa.sa_mask, SIGINT);
  sigaddset(&sa.sa_mask, SIGUSR2);
  sigaddset(&sa.sa_mask, SIGTERM);
  sigaddset(&sa.sa_mask, SIGQUIT);
  sigaddset(&sa.sa_mask, SIGUSR1);
  /*Applico maschera a questo thread. La maschera poi viene ereditata dai figli*/
  pthread_sigmask(SIG_BLOCK, &sa.sa_mask, NULL);

  int fallito;
  pthread_t gestore_segnali_id;
  aS->st = da->StatFileName;
  aS->fd = skt_id;
  fallito = pthread_create(&gestore_segnali_id, NULL, &gestore_segnali, (void*)aS);

  if (fallito){
      perror("Errore nella creazione del Thread Gestore Segnali");
      return -1;
  }
  /*---- fine gestione ----*/

  if( da->StorageSize == 0) da->StorageSize = 10000;
  /*Creazione tabella hash*/
  hashTable = icl_hash_create( da->StorageSize, long_hash, compare);


  pool = initPool(da->ThreadsInPool);

printf("- - - accept - - -\n");
  while ((skt_id1 = accept(skt_id, NULL, 0)) > 0 ) {

    int do_insert = 1;
    pthread_mutex_lock(&lockList);
    if (lenList(&fd_list) == da->MaxConnections) {
      message_hdr_t reply = { .op = OP_FAIL, .key = 0};
      writeHeader(skt_id1, &reply);
      close(skt_id1);
      do_insert = 0;
    } else {
      insertList( &fd_list, skt_id1);
    }
    pthread_mutex_unlock(&lockList);

    if( do_insert && (insertJob(pool, &worker, (void*)skt_id1) != 0)){
      break;
    }
  }
  pthread_mutex_lock(&lockList);
  shutdownList(&fd_list);
  freeList(&fd_list);
  pthread_mutex_unlock(&lockList);

  if(icl_hash_destroy(hashTable, NULL, freeData) != 0){
    fprintf(stderr, "errore free hashTable\n");
    return -1;
  }
  pthread_join(gestore_segnali_id, NULL);
  free(da);
  free(aS);

  return 0;
}

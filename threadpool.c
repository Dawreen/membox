#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>

#include "threadpool.h"
#include "queue.h"

/**
 * struttura dei task che la threadpool deve svolgere
 */
typedef struct job_s{
  void* (*fun)(void*);
  void* arg;
} job_t;

/**
 * arTh array dei thread
 * dimAr dimensione dell'array dei thread
 * jobQ queue con i task che la threadpool deve svolgere
 * lock mutex per bloccare la threadpool
 * jobCond condition variable per gestire gli accessi alla jobQ
 * stopJob variabile che permette di interrompere l'acquisizione di nuovi task
 */
struct threadpool_s{
  pthread_t *arTh;
  unsigned int dimAr;
  queue_t *jobQ;

  pthread_mutex_t lock;
  pthread_cond_t jobCond;

  int stopJob;
};

void *thDo(void *arg){
  threadpool_t *pool = (threadpool_t*)arg;

  sigset_t sign;
  if(sigfillset(&sign) < 0) return NULL;
  if(pthread_sigmask(SIG_BLOCK, &sign, NULL) != 0) return NULL;
  pthread_mutex_lock(&(pool->lock));
  while( pool->stopJob == 0){
    while((length(pool->jobQ) == 0) && pool->stopJob == 0)
      pthread_cond_wait(&(pool->jobCond), &(pool->lock));
    if(pool->stopJob != 0) break;
    job_t *job = (job_t*)pop(pool->jobQ);
    pthread_mutex_unlock(&(pool->lock));
    job->fun(job->arg);
    free(job);
    pthread_mutex_lock(&(pool->lock));
  }
  pthread_mutex_unlock(&(pool->lock));
printf("morte thread %d\n", pthread_self());
  return NULL;
}


threadpool_t *initPool(unsigned int dim){
  threadpool_t *pool = calloc(1, sizeof(threadpool_t));
  if( pool == NULL){
    return NULL;
  }

  //inizializzazione della variabile di condizione e della mutex
  int r;
  if( (r = pthread_mutex_init(&(pool->lock), NULL)) != 0){
    free(pool);
    errno = r;
    return NULL;
  }
  if( (r = pthread_cond_init(&(pool->jobCond),NULL)) != 0){
    pthread_mutex_destroy(&(pool->lock));
    free(pool);
    errno = r;
    return NULL;
  }

  //allocazione della coda dei lavori
  pool->jobQ = initQueue();
  if( pool->jobQ == NULL){
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->jobCond));
    free(pool);
    return NULL;
  }

  //allocazione dell'array dei threads
  pool->dimAr = dim;
  pool->arTh = calloc(dim, sizeof(pthread_t));
  if( pool->arTh == NULL){
    pthread_mutex_destroy(&(pool->lock));
    pthread_cond_destroy(&(pool->jobCond));
    deleteQueue(pool->jobQ);
    free(pool);
    return NULL;
  }

  //creazione thread nella pool
  int i;
  for( i = 0; i < dim; i++){
    if( (r = pthread_create(&((pool->arTh)[i]), NULL, thDo, (void*)pool)) != 0){
      pthread_mutex_destroy(&(pool->lock));
      pthread_cond_destroy(&(pool->jobCond));
      deleteQueue(pool->jobQ);
      free(pool);
      //join dei thread creati fin ora
      int j;
      for( j = 0; j < i; j++){
        pthread_join((pool->arTh)[j], NULL);
      }
      errno = r;
      return NULL;
    }
  }
  return pool;
}

/**
 * @function freePool
 * @brief libera dalla memoria la threadpool passata come parametro
 *
 * @param pool puntatore alla threadpool da liberare
 */
 void freePool(threadpool_t *pool, int condTerm){
   if( pool != NULL){
     if( condTerm && length(pool->jobQ) > 0 ){
       while( length(pool->jobQ) > 0)
       ;
     }

     pthread_mutex_lock(&(pool->lock));

     pool->stopJob = 1;

     pthread_cond_broadcast(&(pool->jobCond));
     pthread_mutex_unlock(&(pool->lock));

     //join di tutti i thread
     int i;
     for( i = 0; i < pool->dimAr; i++)
       pthread_join((pool->arTh)[i], NULL);

     deleteQueue( pool->jobQ);
     free(pool->arTh);
     pthread_mutex_destroy(&(pool->lock));
     pthread_cond_destroy(&(pool->jobCond));

     free(pool);
   }
}

/**
 * @function insertJob
 * @brief inserisce un nuovo task da far compiere alla threadpool
 *
 * @param pool puntatore alla threadpool che deve svolgere il task
 * @param fun funzione da svolgere
 * @param arg argomenti della funzione
 *
 * @return 0 in caso di successo
 *         -1 in caso di fallimento
 */
int insertJob(threadpool_t *pool, void* (*fun)(void*), void* arg){
  if( fun == NULL){
    return -1;
  }
  if(pthread_mutex_lock(&(pool->lock)) != 0) return -1;
  if( pool->stopJob == 0){
    job_t *j = malloc(sizeof(job_t));
    if( j == NULL){
      pthread_mutex_unlock(&(pool->lock));
      return -1;
    }
    j->fun = fun;
    j->arg = arg;
    if(push(pool->jobQ, (void*)j) != 0){
      pthread_mutex_unlock(&(pool->lock));
      free(j);
      return -1;
    }
    pthread_cond_signal(&(pool->jobCond));
    pthread_mutex_unlock(&(pool->lock));
    return 0;
  }
  errno = EACCES;
  return -1;
}

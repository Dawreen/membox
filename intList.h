#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>

typedef struct list_s{
  long int el;
  struct list_s* next;
}list_t;

int insertList( list_t **l, long int key){
  list_t *aux = (list_t*)calloc( 1, sizeof(list_t));
  if( aux == NULL) return -1;
  aux->el = key;
  aux->next = *l;
  *l = aux;
  return 0;
}

int deleteList( list_t **l, long int key){
  list_t *prec = NULL;
  list_t *corr = *l;
  while( corr != NULL){
    if( corr->el != key){
      prec = corr;
      corr = corr->next;
    }else{
      if( prec == NULL){
        *l = corr->next;
      }else{
        prec->next = corr->next;
      }
      free(corr);
      return 0;
    }
  }
  return -1;
}

int shutdownList( list_t **l){
  list_t *aux = *l;
  int res = 0;
  while (aux != NULL) {
    if(shutdown(aux->el, SHUT_RDWR) != 0){
      res = -1;
    }
    aux = aux->next;
  }
  return res;
}

long unsigned int lenList( list_t **l){
  list_t *aux = *l;
  long unsigned int res = 0;
  while (aux != NULL) {
    aux = aux->next;
    res++;
  }
  return res;
}

void freeList( list_t **l){
  list_t *aux = *l;
  while( *l != NULL){
    aux = (*l)->next;
    free(*l);
    *l = aux;
  }
}

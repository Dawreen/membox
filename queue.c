#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <assert.h>

#include "queue.h"

static pthread_mutex_t qlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  qcond = PTHREAD_COND_INITIALIZER;

queue_t *initQueue() {
  queue_t *q = malloc(sizeof(queue_t));
  if (q == NULL){
    return NULL;
  }
  q->head = malloc(sizeof(node_t));
  if (q->head == NULL){
    free(q);
    return NULL;
  }
  q->head->data = NULL;
  q->head->next = NULL;
  q->tail = q->head;
  q->qlen = 0;
  return q;
}

void deleteQueue(queue_t *q) {
  while(q->head != q->tail) {
    node_t *p = (node_t*)q->head;
    q->head = q->head->next;
	  free((void*)p);
  }
  if(q->head) free((void*)q->head);
  free(q);
}

int push(queue_t *q, void *data) {
  assert(data != NULL);
  node_t *n = malloc(sizeof(node_t));
  if( n == NULL) return -1;
  n->data = data;
  n->next = NULL;
  pthread_mutex_lock(&qlock);
  q->tail->next = n;
  q->tail = n;
  q->qlen += 1;
  pthread_cond_signal(&qcond);
  pthread_mutex_unlock(&qlock);
  return 0;
}

void *pop(queue_t *q) {
  pthread_mutex_lock(&qlock);
  while(q->head == q->tail) {
    pthread_cond_wait(&qcond, &qlock);
  }
  // locked
  assert(q->head->next);
  node_t *n = (node_t *)q->head;
  void *data = (q->head->next)->data;
  q->head = q->head->next;
  q->qlen -= 1;
  assert(q->qlen>=0);
  pthread_mutex_unlock(&qlock);
  free((void*)n);
  return data;
}

unsigned int length(queue_t *q) {
  unsigned int len = q->qlen;
  return len;
}

#include <stdio.h>
#include <pthread.h>

#include "pss.h"
#include "pedir.h"


// Defina aca sus variables globales
pthread_mutex_t m;
Queue *q[2];
int cat0; int cat1;

typedef struct {
  int ready;
  int cat;
  pthread_cond_t w;
} Request;

void iniciar() {
  pthread_mutex_init(&m, NULL);
  q[0] = makeQueue();
  q[1] = makeQueue();
  cat0 = 0;
  cat1 = 0;
}

void terminar() {
  destroyQueue(q[0]);
  destroyQueue(q[1]);
  pthread_mutex_destroy(&m);
  cat0 = 0;
  cat1 = 0;
}

void pedir(int cat) {
  pthread_mutex_lock(&m);
  if(!cat0 && !cat1){
    if(cat){
      cat1=1;
    }
    else{
      cat0=1;
    }
  }
  else{
    Request req ={0, cat, PTHREAD_COND_INITIALIZER};
    put(q[cat], &req);
    while(!req.ready){
      pthread_cond_wait(&req.w,&m);
    }
  }
  pthread_mutex_unlock(&m);
}

void devolver() {
  pthread_mutex_lock(&m);
  Request *pr;
  if(cat0){ //el recurso lo tiene cat0
    pr = get(q[1]); //saca el primer elemento de cat1 en espera
    if(pr==NULL){ //no hay cat1 en espera
      cat1=0;
      pr = get(q[0]); //entonces saca el primer elemento de cat0 en espera
      if (pr==NULL){
        cat0=0;
        pthread_mutex_unlock(&m);
        return;
      }
    }
  }
  else if(cat1){ //el recurso lo tiene cat1
    //printf("saca de la cola un cat0 \n");
    pr = get(q[0]); //saca el primer elemento de cat0 en espera
    if(pr==NULL){ //no hay cat0 en espera
      cat0=0;
      pr = get(q[1]); //entonces saca el primer elemento de cat1 en espera
      if (pr==NULL){
        cat1=0;
        pthread_mutex_unlock(&m);
        return;
      }
    }
  }
  else{
    pthread_mutex_unlock(&m);
    return;
  }
  pr->ready=1;
  if (pr->cat==0){ //dejamos registro de quien tiene el recurso
    cat0=1;
    cat1=0;
  }
  else if(pr->cat==1){
    cat0=0;
    cat1=1;
  }
  pthread_cond_signal(&pr->w);
  pthread_mutex_unlock(&m);
}

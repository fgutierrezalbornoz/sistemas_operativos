#include <pthread.h>
#include <stdio.h>
#include "compartir.h"

typedef struct{
  void *p;
} Puntero;
Puntero puntero;
int n_accede=0;
int n_devuelto=0;
int compartiendo=0;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void compartir(void *ptr) {
  pthread_mutex_lock(&m);
  puntero.p=ptr;
  compartiendo=1;
  pthread_cond_broadcast(&cond); //debe despertar a acceder
  //entra en wait si no se ha ejecutado la misma cantidad de veces acceder y compartir
  // o cuando no se ha ejecutado antes acceder
  while(n_accede==0 || n_devuelto!=n_accede){ 
    pthread_cond_wait(&cond,&m);
  }
  compartiendo=0;
  n_accede=0;
  n_devuelto=0;
  pthread_mutex_unlock(&m);
}

void *acceder(void) {
  pthread_mutex_lock(&m);
  n_accede++;
  while(!compartiendo){ //entra en wait si no se ha ejecutado compartir
    pthread_cond_wait(&cond,&m);
  }
  void* res=puntero.p;
  pthread_mutex_unlock(&m);
  return res;
}

void devolver(void) {
  pthread_mutex_lock(&m);
  n_devuelto++;
  //si se ha ejecutado la misma cantidad de veces acceder y devolver
  //despierta a compartir
  if (n_accede==n_devuelto){
    pthread_cond_broadcast(&cond);//debe despertar a compartir
  }
  pthread_mutex_unlock(&m);
}

#define _XOPEN_SOURCE 500

#include "nthread-impl.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <nthread.h>

#include "pss.h"
#include "disk.h"

// Defina aca tipos y variables globales

nThread disco;
int ocupado = 0;
int U;
PriQueue *q1;
PriQueue *q2;



void iniDisk(void) {
  q1 = makePriQueue();
  q2 = makePriQueue();
}

void cleanDisk(void) {
  destroyPriQueue(q1);
  destroyPriQueue(q2);
}

void nRequestDisk(int track, int delay) {
  
  START_CRITICAL
  if (!ocupado){ //no está ocupado, le paso el disco
    ocupado=1; // marco el disco como ocupado
    U=track; //guardo la pista
  }
  else{ //está ocupado, debe esperar
    nThread thisTh= nSelf();
    int *intPtr = malloc(sizeof(int));
    *intPtr = track;
    thisTh->ptr = intPtr; //guardo la pista en el thread por si acaso
    if(track>=U){
      priPut(q1, thisTh, track); //pongo el thread en la cola junto a su prioridad (pista)
    }
    else{
      priPut(q2, thisTh, track);
    }
    suspend(WAIT_REQUEST); //suspendo hasta que se desocupe el disco
    schedule();
  }
  END_CRITICAL
  return;
}

void nReleaseDisk() {
  START_CRITICAL
  ocupado=0;
  if(!emptyPriQueue(q1)){ // primera cola no vacía
    ocupado = 1; //el disco está ocupado
    nThread pr = priGet(q1); //extrae un elemento de la cola
    U = *(int *)pr->ptr;
    setReady(pr); //lo setea en ready
    schedule(); //le entrego el core al que sigue
  }
  else{
    //primera cola vacía
    //intercambia colas
    PriQueue *q = q1;
    q1 = q2;
    q2 = q;
    if(!emptyPriQueue(q1)){
      ocupado = 1;
      nThread pr2 = priGet(q1);
      U = *(int *)pr2->ptr;
      setReady(pr2);
      schedule();
    }
  }
  END_CRITICAL
  return;
}
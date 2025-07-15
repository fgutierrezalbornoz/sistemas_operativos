#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "spinlocks.h"
#include "pss.h"
#include "subasta.h"

struct subasta {
  int n; //n productos ofrecidos
  int status; //1 abierto 0 cerrada
  PriQueue *compradores; //cola de compradores
  int sls; //spinlock de la subasta
};

struct Oferta{
  int slo; //spinlock de la oferta
  double precio; //precio de la oferta
  int status; //0: rechazada 1: adjudicada
};

// Defina aca otras estructuras si las necesita

Subasta nuevaSubasta(int unidades) {
  struct subasta *sub = malloc(sizeof(struct subasta));
  sub->n=unidades;
  sub->status=1;
  sub->sls=OPEN;
  sub->compradores=makePriQueue();
  return sub;
}

void destruirSubasta(Subasta s) {
  destroyPriQueue(s->compradores);
  free(s);
}

int ofrecer(Subasta s, double precio) {
  spinLock(&s->sls);
  
  if (!s->status){ //si la subasta está cerrada, no acepta la oferta
    spinUnlock(&s->sls);
    return 0;
  }
  if (priLength(s->compradores)<s->n){//si hay menos ofertas que productos ofrecidos

    struct Oferta *oferta = malloc(sizeof(struct Oferta)); //se crea la nueva oferta
    oferta->precio = precio;
    oferta->status = 0;
    oferta->slo = CLOSED;

    priPut(s->compradores, oferta, precio); //se agrega a los mejores oferentes
    spinUnlock(&s->sls); //se libera el spinlock de la subasta
    spinLock(&oferta->slo); //se toma el spinlock de la oferta.
    int resultado = oferta->status;//guarda resultado de la oferta
    free(oferta); //libera memoria
    return resultado;
  }
  else{ //si hay la misma cantidad de oferentes aceptados que items ofrecidos se chequea una condición
    if (precio>priBest(s->compradores)){ //la nueva oferta es mejor que la menor que ya había
      struct Oferta *peor_oferta = priGet(s->compradores); //se elimina la oferta más baja considerada hasta ahora
      peor_oferta->status = 0;
      spinUnlock(&peor_oferta->slo); //se libera la oferta desechada 
      struct Oferta *oferta = malloc(sizeof(struct Oferta)); //se crea la nueva oferta
      oferta->precio = precio;
      oferta->status = 0;
      oferta->slo = CLOSED;
      priPut(s->compradores, oferta, precio); //se agrega la oferta nueva a los mejores oferentes
      spinUnlock(&s->sls); //se libera el spinlock de la subasta
      spinLock(&oferta->slo); //se toma el spinlock de la nueva oferta
      int resultado = oferta->status; //guarda resultado de la oferta
      free(oferta); //libera memoria
      return resultado;
    }
    else{ //la nueva oferta es peor o igual que una ya considerada
      spinUnlock(&s->sls);
      return 0;
    }
  }
}

double adjudicar(Subasta s, int *punidades) {
  spinLock(&s->sls);
  s->status=0; //la oferta se marca como cerrada
  double recaudado = 0.0;
  *punidades = (s->n)-priLength(s->compradores); //Se calculan los items restantes
  while(!emptyPriQueue(s->compradores)){ //se recorren las ofertas que se adjudican los items
    struct Oferta *adjudicado = priGet(s->compradores); //se obtiene la oferta
    recaudado += adjudicado->precio; //se suma lo ofertado
    adjudicado->status = 1; //se cambia el resultado de la oferta a adjudicada
    spinUnlock(&adjudicado->slo); //se libera el spinlock de la oferta
  }
  spinUnlock(&s->sls);
  return recaudado;
}

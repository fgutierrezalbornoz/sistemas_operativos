#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <float.h>
#include <stdint.h>
#include <sys/time.h>

#include "viajante.h"

#define PHI 0x9e3779b9


/* 
    métodos y estructuras necesarias para que funcione el código
*/
typedef struct {
  uint32_t Q[4096], c; // Para rand_cmwc
  int32_t cur;         // Para random0or1
  int bit;
} RandGen;

static void init_rand(RandGen *gen, uint32_t seed) {
    uint32_t x= seed;
    int i;

    gen->c= 362436;
    gen->Q[0] = x;
    gen->Q[1] = x + PHI;
    gen->Q[2] = x + PHI + PHI;

    for (i = 3; i < 4096; i++)
            gen->Q[i] = gen->Q[i - 3] ^ gen->Q[i - 2] ^ PHI ^ i;
}

static uint32_t rand_cmwc(RandGen *gen) {
    uint64_t t, a = 18782LL;
    uint32_t i = 4095;
    uint32_t x, r = 0xfffffffe;
    i = (i + 1) & 4095;
    t = a * gen->Q[i] + gen->c;
    gen->c = (t >> 32);
    x = t + gen->c;
    if (x < gen->c) {
            x++;
            gen->c++;
    }
    return (gen->Q[i] = r - x);
}

static void gen_ruta_alea(int x[], int n, RandGen *pgen) {
  x[0]= 0;
  for (int i= 1; i<=n; i++) {
    x[i]= i;
  }
  for (int i= 1; i<n; i++) { // para el caso i==n, intercambiaria x[n] y x[n]
    int r= rand_cmwc(pgen)%(n-i+1)+i; // elige intercambiar x[i] con x[r]
    int tmp= x[i];
    x[i]= x[r];
    x[r]= tmp;
  }
}

static double dist(int z[], int n, double **m) {
  double d= m[z[n]][0];    // distancia de z[n] a 0
  for (int i=0; i<n; i++) {
      d += m[z[i]][z[i+1]];
  }
  return d;
}

/*
    viajante secuencial
*/

double viajante_sec(int z[], int n, double **m, int nperm) {
    RandGen gen;
    init_rand(&gen, rand());
    double min= DBL_MAX; // la menor distancia hasta el momento
    for (int i= 1; i<=nperm; i++) {
        int x[n+1];
        // almacenará una ruta aleatoria
        gen_ruta_alea(x, n, &gen); // genera ruta x[0]=0, x[1], x[2], ..., x[n], x[0]=0
        // calcula la distancia al recorrer 0, x[1], ..., x[n], 0
        double d= dist(x, n, m);
        if (d<min) {
            // si distancia es menor que la que se tenía hasta el momento
            min= d;
            // d es la nueva menor distancia
            for (int j= 0; j<=n; j++)
                z[j]= x[j]; // guarda ruta que recorre la menor distancia en parámetro z
    } }
    return min;
}

/*
    Estructura que contiene la info del camino entre ciudades
*/

typedef struct{
    int *z;
    int n;
    double **m;
    int nperm;
    double ret;
} Camino;

/*
    método que ejecuta el thread
*/

void *thread(void *p){
    Camino *camino = (Camino *)p;
    int *z = camino->z;
    int n = camino->n;
    double **m = camino->m;
    int nperm = camino->nperm;

    camino->ret = viajante_sec(z, n, m, nperm);
    return NULL;
}

/*
    viajante paralelo
*/

double viajante_par(int z[], int n, double **m, int nperm, int p){
    pthread_t pid[p];
    Camino camino[p];

    for (int k = 0; k < p; k++){
        int *zk=malloc((n+1)*sizeof(int));
        camino[k].z=zk;
        camino[k].n=n;
        camino[k].m=m;
        camino[k].nperm = nperm / p;
        pthread_create(&pid[k], NULL, thread, &camino[k]);
    }

    double min= DBL_MAX;

    for (int k = 0; k < p; k++){
        pthread_join(pid[k], NULL);
        if (camino[k].ret < min){
            min = camino[k].ret;
            for (int j= 0; j<=n; j++)
                z[j]= camino[k].z[j];
        }
        free(camino[k].z);
    }
    return min;
}
#define _XOPEN_SOURCE 500

#include <stdio.h>
#include <math.h>
#include <float.h>
#include <sys/time.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <pthread.h>

#include "viajante.h"

#define N_INTENTOS 4
#ifdef OPT
#define TOLERANCIA 1.7
#else
#define TOLERANCIA 0.0
#endif


// ----------------------------------------------------
// Funcion que entrega el tiempo transcurrido desde el lanzamiento del
// programa en milisegundos

static long long getUSecsOfDay() {
    struct timeval Timeval;
    gettimeofday(&Timeval, NULL);
    return (long long)Timeval.tv_sec*1000000+Timeval.tv_usec;
}

static int time0= 0;

static int getTime0() {
    return getUSecsOfDay()/1000;
}

static void resetTime() {
  time0= getTime0();
}

static int getTime() {
  return getTime0()-time0;
}

// ----------------------------------------------------
// Generacion de numeros aleatorios
//
// Es correcto usar la funcion estandar random, que genera buenos
// numeros aleatorios.  Pero es ineficiente.  La funcion random pide un
// mutex para evitar dataraces.  Pero este problema invoca random
// demasiado frecuentemente, provocando asi tanta contencion en la solucion
// paralela que hasta se podria demorar mas que la secuencial.
// Por eso tuve que usar otra version de la generacion de
// numeros aleatorios, que no sea compartida por los distintos threads.
// Es decir cada thread genera su propia secuencia de numeros.  El
// estado necesario para la generacion de los numeros se almacena en la
// estructura RandGen.  Cada thread maneja su propia instancia de RandGen
// para evitar dataraces.  Aun asi la semilla inicial para cada instancia
// de RandGen se genera con random.

#define MAXBITS 31

// Sacado de: https://stackoverflow.com/questions/9492581/c-random-number-generation-pure-c-code-no-libraries-or-functions

#define PHI 0x9e3779b9

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

// ----------------------------------------------------
// Generacion de n ciudades aleatorias en un cuadrado de 1 x 1
// Las ciudades estan en un desierto, de modo que siempre hay un
// camino en linea recta desde una ciudad a otra.

double **repartir_ciudades(int n) {
  double x[n+1], y[n+1];
  for (int i= 0; i<=n; i++) {
    x[i] = random() / ((double)RAND_MAX+1);
    y[i] = random() / ((double)RAND_MAX+1);
  }
   
  double **m= malloc((n+1)*sizeof(double*));
  for (int i= 0; i<=n; i++) {
    m[i]= malloc((n+1)*sizeof(double));
    for (int j= 0; j<=n; j++) {
      double dx= x[i]-x[j];
      double dy= y[i]-y[j];
      m[i][j]= sqrt(dx*dx+dy*dy);
    }
  }

  return m;
}

void liberar_ciudades(double **m, int n) {
  for (int i= 0; i<=n; i++)
    free(m[i]);
  free(m);
}

// ----------------------------------------------------
// Version secuencial de viajante

static double dist(int z[], int n, double **m) {
  double d= m[z[n]][0];    // distancia de z[n] a 0
  for (int i=0; i<n; i++) {
      d += m[z[i]][z[i+1]];
  }
  return d;
}

static void printRuta(int z[], int n) {
  for(int i=0; i<=n; i++) {
    printf("%d ", z[i]);
  }
  printf("0\n");
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

double viajante(int z[], int n, double **m, int nperm) {
  RandGen gen;
  init_rand(&gen, random());
  double min= DBL_MAX; // la distancia mas corta hasta el momento
  uint32_t primer_rand= rand_cmwc(&gen);
  for (int i= 1; i<=nperm; i++) {
    int x[n+1];          // almacenara una ruta aleatoria
    gen_ruta_alea(x, n, &gen); // genera ruta x[0]=0, x[1], ..., x[n], x[0]=0
    // calcula la distancia al recorrer 0, x[1], ..., x[n], 0
    double d= dist(x, n, m);
    if (d<min) {    // si distancia es menor a la mas corta hasta el momento
      min= d;       // d es la nueva distancia mas corta
      for (int j= 0; j<=n; j++)
        z[j]= x[j]; // guarda ruta mas corta en parametro z
  } }
  static pthread_mutex_t mutex= PTHREAD_MUTEX_INITIALIZER;
  pthread_mutex_lock(&mutex);
  printf("Resultados viajante: nperm=%d min=%f primer_rand=%u\n",
         nperm, min, primer_rand);
  printRuta(z, n);
  pthread_mutex_unlock(&mutex);
  return min;
}

int main() {

  int minimo_no_satisfactorio= 1;
  srandom(1000000000); // para que resultado sea deterministico
  
  printf("Test 1: ejemplo con solo 6 ciudades\n");
  {
#ifndef VALGRIND
    int k= 8000000;
#else
    int k= 800000;
#endif
    int n= 5;
    int p= 4;
    double **m= repartir_ciudades(n);
    int z[n+1];
    printf("Calculando secuencialmente\n");
    double bestsec= viajante(z, n, m, k);
    printf("Calculando en paralelo\n");
    double bestpar= viajante_par(z, n, m, k, p);
    printf("Resultado final: nperm=%d distancia minima=%f\n", k, bestpar);
    printRuta(z, n);
    if (fabs(dist(z, n, m)-bestpar)>0.00000000000001) {
      fprintf(stderr, "La distancia entregada es incorrecta para esa ruta\n");
      exit(1);
    }
    if (fabs(bestsec-bestpar)>0.00000000001) {
      fprintf(stderr, "La solucion debio ser %f, no %f\n", bestsec, bestpar);
      exit(1);
    }
    printf("Test 1 aprobado\n");
    printf("\n--------------------------------------------------\n\n");
    liberar_ciudades(m, n);
  }

  printf("Test 2: Un ejemplo grande con 33 ciudades\n");
  {
#ifdef OPT
    int k= 10000000;
#else
    int k= 1000000;
#endif
    int n= 32;
    int p= 8;
    double **m= repartir_ciudades(n);
    int z[n+1];

    double bestsec, bestpar;

    int tiempo_sec, tiempo_par;
    double speedUp= 0;
    int i;
    printf("Precalentando secuencialmente\n");
    bestsec= viajante(z, n, m, k/100);
 
    printf("Calculando secuencialmente con k= %d permutaciones\n", k);
    resetTime();
    bestsec= viajante(z, n, m, k);
    tiempo_sec= getTime();
    printf("\nResultado secuencial Tiempo=%d distancia minima=%f\n",
           tiempo_sec, bestsec);
 
    for (i= 1; i<=N_INTENTOS; i++) {
      printf("\n-----\n");
      printf("Calculando en paralelo con k= %d permutaciones, intento %d\n",
             k, i);
      resetTime();
      bestpar= viajante_par(z, n, m, k, p);
      tiempo_par= getTime();
      speedUp= (double)tiempo_sec/tiempo_par;
      printf("\nResultado paralelo: Tiempo=%d speedup=%f distancia minima=%f\n",
           tiempo_par, speedUp, bestpar);
      printRuta(z, n);
 
      if (fabs(dist(z, n, m)-bestpar)>0.00000000000001) {
        fprintf(stderr, "La distancia entregada es incorrecta para esa ruta\n");
        exit(1);
      }
      if (fabs(bestpar-bestsec)>1) {
        fprintf(stderr, "La solucion debio rondar %g, no %g\n",
                bestsec, bestpar);
        minimo_no_satisfactorio= 0;
      }
      if (speedUp>=TOLERANCIA)
        break;
    }
    if (i>N_INTENTOS) {
      fprintf(stderr, "Despues de %d intentos no obtuvo un speedup de %gx\n",
              N_INTENTOS, TOLERANCIA);
      fprintf(stderr, "Si ejecuto con make test-O revise la paralelizacion.\n");
      liberar_ciudades(m, n);
      exit(1);
    }
    printf("Test 2 aprobado: speedup >= %gx\n\n", TOLERANCIA);
    liberar_ciudades(m, n);
  }

  if (minimo_no_satisfactorio)
    printf("Felicitaciones: su tarea funciona\n");
  else {
    fprintf(stderr, "El minimo calculado no es satisfactorio. Revise.\n");
    exit(1);
  }
  
  return 0;
}

#define _DEFAULT_SOURCE 1

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <nthread.h>
#include <sys/time.h>

#include "pedir.h"

#define NCORES 8
#define TIMEOUT 10000
#define OCUP 100

#pragma GCC diagnostic ignored "-Wunused-function"

static int tiempoActual();
static void pausa(int tiempo_espera);
static char *indent(int t);
static int th_fun(int tpedir, int num_th, int cat, int tadq, int tocup);

// Tipos y operaciones de nSystem

typedef pthread_t Core;

static Core nEmitTask( int (*proc)(), ... );
static void nExitTask(int rc);
static int nWaitTask(Core t);

typedef struct monitor *nMonitor;
static nMonitor nMakeMonitor();
static void nEnter(nMonitor m);
static void nExit(nMonitor m);
static void nWait(nMonitor m);
static void nNotifyAll(nMonitor m);
static void nDestroyMonitor(nMonitor m);

#define nMalloc malloc
#define nFree free

static void nSleep(int millis);

/*************************************************************
 * Funcion que entrega el tiempo transcurrido desde el lanzamiento del
 * programa en milisegundos
 *************************************************************/

static int time0= 0;

static int getTime0() {
    struct timeval Timeval;
    gettimeofday(&Timeval, NULL);
    return Timeval.tv_sec*1000+Timeval.tv_usec/1000;
}

void resetTime() {
  time0= getTime0();
}

int getTime() {
  return getTime0()-time0;
}

/*************************************************************
 * Manejo de pausas
 *************************************************************/

static int tiempo_actual= 0;
nMonitor t_mon;

static int tiempoActual() {
  nEnter(t_mon);
  int t= tiempo_actual;
  nExit(t_mon);
  return t;
}

static void pausa(int tiempo_espera) {
  nEnter(t_mon);
  int tiempo_inicio= tiempo_actual;
  nExit(t_mon);
  nSleep(tiempo_espera*1000);
  nEnter(t_mon);
  tiempo_actual= tiempo_inicio+tiempo_espera;
  nExit(t_mon);
}

/*************************************************************
 * Indentacion
 *************************************************************/

static char *espacios="                                                  ";

static char *indent(int t) {
  int len= strlen(espacios);
  return &espacios[len-(t-1)*10];
}

/*************************************************************
 * nEmitTask, nExitTask, nWaitTask
 *************************************************************/

typedef struct InfoEmit {
  int  rdy;
  pthread_mutex_t m;
  pthread_cond_t c;
  va_list ap;
  int (*proc)();
}
  InfoEmit;

static void *TaskInit(void *info);

static Core nEmitTask( int (*proc)(), ... ) {
  /* (un procedimiento puede declarar mas argumentos que la cantidad
   * de argumentos con que es llamado)
   */
  InfoEmit info= {0, PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER};

  va_list ap;

  /* Los argumentos y el procedimiento se pasaran a TaskInit en info */
  va_start(ap, proc);

  // Modificacion para AMD64. Por Francisco Cifuentes
  va_copy(info.ap, ap);
  info.proc= proc;

  // No se puede invocar va_end mientras core->t no haya sacado los parametros
  pthread_mutex_lock(&info.m);

    pthread_t new_core;
    pthread_create(&new_core, NULL, TaskInit, &info);
    while (!info.rdy)
      pthread_cond_wait(&info.c, &info.m);

  pthread_mutex_unlock(&info.m);

  va_end(ap);

  return new_core;
}

__attribute__((no_sanitize_address))
static void *TaskInit( void *ptr ) {
  InfoEmit *pinfo= ptr;
  int (*proc)()= pinfo->proc;
  long long  a0= va_arg(pinfo->ap, long long);
  long long  a1= va_arg(pinfo->ap, long long);
  long long  a2= va_arg(pinfo->ap, long long);
  long long  a3= va_arg(pinfo->ap, long long);
  long long  a4= va_arg(pinfo->ap, long long);
  long long  a5= va_arg(pinfo->ap, long long);
  long long  a6= va_arg(pinfo->ap, long long);
  long long  a7= va_arg(pinfo->ap, long long);
  long long  a8= va_arg(pinfo->ap, long long);
  long long  a9= va_arg(pinfo->ap, long long);
  long long a10= va_arg(pinfo->ap, long long);
  long long a11= va_arg(pinfo->ap, long long);
  long long a12= va_arg(pinfo->ap, long long);
  long long a13= va_arg(pinfo->ap, long long);
  // soporta hasta 14 argumentos enteros (o 7 punteros de 64 bits)
  pthread_mutex_lock(&pinfo->m);

    pinfo->rdy= 1;
    pthread_cond_signal(&pinfo->c);

  pthread_mutex_unlock(&pinfo->m);


  // Llama el procedimiento raiz de la tarea
  intptr_t rc= (*proc)(a0, a1, a2, a3, a4, a5, a6, a7, a8
                       , a9, a10, a11, a12, a13
                      );
  return (void*)rc;
}

static void nExitTask(int rc) {
  pthread_exit((void*)(intptr_t)rc);
}

static int nWaitTask(Core core) {
  void *ptr;
  pthread_join(core, &ptr);
  return (intptr_t)ptr;
}

/*************************************************************
 * nMonitor
 *************************************************************/

struct monitor {
  pthread_mutex_t m;
  pthread_cond_t c;
};

static nMonitor nMakeMonitor() {
  nMonitor mon= malloc(sizeof(*mon));
  pthread_mutex_init(&mon->m, NULL);
  pthread_cond_init(&mon->c, NULL);
  return mon;
}

static void nEnter(nMonitor mon) {
  pthread_mutex_lock(&mon->m);
}

static void nExit(nMonitor mon) {
  pthread_mutex_unlock(&mon->m);
}

static void nWait(nMonitor mon) {
  pthread_cond_wait(&mon->c, &mon->m);
}

static void nNotifyAll(nMonitor mon) {
  pthread_cond_broadcast(&mon->c);
}

static void nDestroyMonitor(nMonitor mon) {
  pthread_mutex_destroy(&mon->m);
  pthread_cond_destroy(&mon->c);
  free(mon);
}

/*************************************************************
 * nSleep
 *************************************************************/

void nSleep(int millis) {
  usleep(millis*1000);
}

/*************************************************************
 * Para el test del enunciado
 *************************************************************/

static int th_fun(int tpedir, int num_th, int cat, int tadq, int tocup) {
  char *ind= indent(num_th);
  nPrintf("Tpo=%d:%s T%d invoca pedir(%d)\n",
          tiempoActual(), ind, num_th, cat);
  if (cat>1 || cat<0)
    nFatalError("test", "La categoria %d es incorrecta\n", cat);
  pedir(cat);
  nPrintf("Tpo=%d:%s T%d cat %d obtiene recurso\n",
          tiempoActual(), ind, num_th, cat);
  if (tadq!=tiempoActual()) {
    nFatalError("test", "T%d obtiene recurso en tiempo erroneo\n", num_th);
  }
  pausa(tocup);
  nPrintf("Tpo=%d:%s T%d devuelve recurso\n", tiempoActual(), ind, num_th);
  devolver();
  return 0;
}

// Para el test de robustez

static int dentro= 0;

static int rob_fun(int cat) {
  int fin= 0;
  int k= 0;
  while (!fin) {
    int punto= 0;
    pedir(cat);
    if (dentro==1)
      nFatalError("test", "No se cumple la exclusion mutua\n");
    dentro= 1;
    if (k%10==0) {
      if (getTime()>TIMEOUT)
        fin= 1;
    }
    for (volatile int i=0; i<OCUP; i++)
      ;
    dentro= 0;
    devolver();
    k++;
    if (punto)
      nPrintf(".");
  }
  return k;
}

int main() {
  t_mon= nMakeMonitor();

  iniciar();

#if 1
  nPrintf("El ejemplo del enunciado\n");
  nPrintf("------------------------\n\n");
  // Parametros de th_fun: tiempo de invocacion de pedir, num. thread,
  // categoria, tiempo en que se espera que obtenga recurso,
  // tiempo que toma en devolver el recurso
  Core t1= nEmitTask(th_fun, 0, 1, 0, 0, 4);
  pausa(1);
  Core t3= nEmitTask(th_fun, 1, 3, 0, 6, 1);
  pausa(1);
  Core t2= nEmitTask(th_fun, 2, 2, 1, 4, 2);
  pausa(1);
  Core t5= nEmitTask(th_fun, 3, 5, 0, 8, 1);
  nPrintf("Tpo=%d:%s Esperando que T1 termine\n", tiempoActual(), indent(1));
  nWaitTask(t1);
  nPrintf("Tpo=%d:%s T1 termino\n", tiempoActual(), indent(1));
  pausa(1);
  Core t4= nEmitTask(th_fun, 5, 4, 1, 7, 1);
  pausa(1);
  nPrintf("Tpo=%d:%s Esperando que T2 termine\n", tiempoActual(), indent(2));
  nWaitTask(t2);
  nPrintf("Tpo=%d:%s T2 termino\n", tiempoActual(), indent(2));
  nPrintf("Tpo=%d:%s Esperando que T3 termine\n", tiempoActual(), indent(3));
  nWaitTask(t3);
  nPrintf("Tpo=%d:%s T3 termino\n", tiempoActual(), indent(3));
  nPrintf("Tpo=%d:%s Esperando que T4 termine\n", tiempoActual(), indent(4));
  nWaitTask(t4);
  nPrintf("Tpo=%d:%s T4 termino\n", tiempoActual(), indent(4));
  nPrintf("Tpo=%d:%s Esperando que T5 termine\n", tiempoActual(), indent(5));
  nWaitTask(t5);
  nPrintf("Tpo=%d:%s T5 termino\n", tiempoActual(), indent(5));
#endif

  resetTime();

  nPrintf("\nTest de robustez\n");
  nPrintf("----------------\n\n");
  nPrintf("\nToma un poco mas de 10 segundos\n");
  nPrintf("t1 t2 t3 ... significa que ese thread comenzo a trabajar\n\n");
  // nSetTimeSlice(1);
  Core *cores= nMalloc(NCORES*sizeof(Core));
  int cnt= 0;
  for (int i= 0; i<NCORES; i++)
    cores[i]= nEmitTask(rob_fun, i&1);
  for (int i= 0; i<NCORES; i++)
    cnt += nWaitTask(cores[i]);
  
  nFree(cores);

  terminar();

  nPrintf("\nNumero de entradas/salidas = %d\n", cnt);
  nPrintf("En mi computador make run reporto unas 10 mil "
          "entradas/salidas con 3 threads\n");
  nPrintf("\nFelicitaciones: funciona\n");
  return 0;
}

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include "viajante.h"
# include <float.h>               // Library to use DBL_MAX

// Structure needs to contain all parameters needed for threads
typedef struct {
  int *z_by_thread;               // array with shortest path found by thread
  int n;                          // number of cities
  double **m;                     // matrix of distances
  int nperm;                      // number of permutations
  double min_by_thread;           // shortest path found by thread
} Params;

// Function to be executed by each thread
void* thread(void *p) {
  Params *args = (Params*) p;
  args->min_by_thread = viajante(args->z_by_thread, args->n, args->m, args->nperm);
  return NULL;
}

double viajante_par(int z[], int n, double **m, int nperm, int p) {
  pthread_t pid[p];                                                 // array of thread ids
  Params args[p];                                                   // array of parameters for each thread
  int interval = nperm / p;                                         // number of permutations per thread

  for (int i = 0; i < p; i++) {                                     // create p threads              
    args[i].z_by_thread = (int *)malloc((n + 1) * sizeof(int));     
    args[i].n = n;
    args[i].m = m;
    args[i].nperm = interval;

    pthread_create(&pid[i], NULL, thread, &args[i]);                // create thread i with parameters args[i]
  }

  // Set min to the maximum value of a double so by the end of the loop it will be updated with the shortest path
  double min = DBL_MAX;                                             // initialize min to DBL_MAX
  int best_thread_index = 0;                                        // index of the thread with the shortest path

  // Wait for all threads to finish and update min with the shortest path found
  for (int i = 0; i < p; i++) {
    pthread_join(pid[i], NULL);                                     // wait for thread i to finish
    if (args[i].min_by_thread < min) {                              // if thread i found a shorter path
      min = args[i].min_by_thread;                                  // update min
      best_thread_index = i;
    }
  }

  for (int i = 0; i <= n; i++) {                                   
    z[i] = args[best_thread_index].z_by_thread[i];                  // copy shortest path to z
  }

  for (int i = 0; i < p; i++) {
    free(args[i].z_by_thread);                                  // free memory on each thread
  }

  return min;
}

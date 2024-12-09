#include <pthread.h>
#include "compartir.h"

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;       // Mutex and condition variable to coordinate access to shared data
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void *shared_data = NULL;                                // Shared data pointer

// Control thread states by variables
int active_threads = 0;                                  // Num of threads currently accessing shared data
int waiting_threads = 0;                                 // Num of threads waiting to access shared data
int data_available = 0;                                  // Indicates if shared data is available (1 if available, 0 if not)


void compartir(void *ptr) {
    pthread_mutex_lock(&mutex);                          // Lock the mutex to ensure exclusive access

    while (active_threads > 0) {                         // Wait until there are no active threads using the shared data
        pthread_cond_wait(&cond, &mutex);
    }                                                 
                                                         
    while (active_threads == 0 && waiting_threads == 0 && data_available == 1) {        // Wait until there are no active threads 
        pthread_cond_wait(&cond, &mutex);                                               // using the shared data
    }                                                    

    shared_data = ptr;                                   // Update the pointer to the new shared data 
    data_available = 1;                                  // Signal that data is available

    pthread_cond_broadcast(&cond);                       // Notify all waiting threads that the data is available

    while (data_available == 1) {                        // Wait until all threads have finished processing the data
        pthread_cond_wait(&cond, &mutex);
    }

    pthread_mutex_unlock(&mutex);                        // Unlock the mutex, allowing other threads to proceed
}


void *acceder(void) {
    pthread_mutex_lock(&mutex);                          // Lock the mutex to ensure exclusive access to the data
    waiting_threads++;

    while (!data_available) {                            // Wait until the shared data becomes available
        pthread_cond_wait(&cond, &mutex);
    }

    if (data_available == 1) {                           // If data is available, notify 'compartir' to continue
        pthread_cond_broadcast(&cond);
    }

    waiting_threads--;                                   // Decrease the number of waiting threads 
    active_threads++;                                    // Increase active threads

    void *p = shared_data;                               // Store the pointer to the shared data

    pthread_mutex_unlock(&mutex);                        // Unlock the mutex and return the data
    return p;
}


void devolver(void) {
    pthread_mutex_lock(&mutex);                          // Lock the mutex to modify access state

    active_threads--;                                    // Decrease the number of active threads

    if (active_threads == 0) {                           // If no more threads are active, mark the data as unavailable
        data_available = 0;
    }

    pthread_cond_broadcast(&cond);                       // Notify 'compartir' that all threads have returned the data

    pthread_mutex_unlock(&mutex);                        // Unlock the mutex
}

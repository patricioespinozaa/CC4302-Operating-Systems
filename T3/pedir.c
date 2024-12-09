#include <stdio.h>
#include <pthread.h>
#include "pss.h"
#include "pedir.h"

// Equipo == categoria              // Basado del aux 3 y 4    
// Oponente == categoria opuesta 

// Definir variables globales
pthread_mutex_t mutex;
pthread_cond_t cond[2];                        // Condiciones para los threads de categoría 0 y 1
Queue *queues[2];                              // Cond. para las colas FIFO para los threads en espera por categoría -> alternar categorias
int adentro[2] = {0, 0};                       // Contar cantidad de threads dentro del recurso por categoría
static int ultimo_cat = 1;                     // Para llevar el registro del último thread que tuvo el recurso

//Patron Request
typedef struct {
    int equipo;                                 // La categoria del thread (0 o 1)
    pthread_cond_t cond;                        // Condición individual para cada thread para realizar el orden de llegada -> 
} Request;                                          // -> se despierta solo al primero de la cola                               

// Inicializar var. globales
void iniciar() {
    // Inicializar el mutex y las condiciones para las categorias globales
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond[0], NULL);          // Cat 0
    pthread_cond_init(&cond[1], NULL);          // Cat 1

    // Inicializar las colas para los threads en espera
    queues[0] = makeQueue();                    // Cat 0
    queues[1] = makeQueue();                    // Cat 1
}

// Liberar los recursos
void terminar() {
    // Destruir el mutex y las condiciones inicializadas en iniciar()
    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond[0]);
    pthread_cond_destroy(&cond[1]);

    // Liberar las colas globales inicializadas en iniciar()
    destroyQueue(queues[0]);
    destroyQueue(queues[1]);
}

// Pedir el recurso unico
void pedir(int cat) {
    int oponente = !cat;                        // Determinar la categoria opuesta
    pthread_mutex_lock(&mutex);                 // Entrar en la zona critica

    // Si hay threads de la categoría opuesta o están esperando, el thread actual debe esperar
    if (adentro[oponente] > 0 || !emptyQueue(queues[oponente]) || adentro[cat] > 0) {
        // Crear una solicitud Request para este thread
        Request req;
        req.equipo = cat;
        pthread_cond_init(&req.cond, NULL);     // Inicializar la condición para el thread
        put(queues[cat], &req);                 // Poner la request en la cola

        // Esperar hasta ser despertado
        pthread_cond_wait(&req.cond, &mutex);

        // Luego de ser despertado, destruir la condicion
        pthread_cond_destroy(&req.cond);
    } else {
        // Si no hay oponentes ni mas threads de la misma categoría esperando podemos entrar al recurso directamente
        adentro[cat]++;
    }

    pthread_mutex_unlock(&mutex);               // Salir de la zona critica
}

// Devolver el recurso unico soliticado por un thread
void devolver() {
    pthread_mutex_lock(&mutex);                 // Entrar en la zona crítica
    
    // Determinar la categría del thread que esta devolviendo el recurso
    int cat;
    if (adentro[0] > 0) {
        cat = 0;
    } else {
        cat = 1;
    }
     
    adentro[cat]--;                             // El thread de la categoria actual libera el recurso
    int oponente = !cat;                        // Categoria opuesta

    // Si no queda nadie del equipo dentro se le asigna el recurso a la categoría opuesta si hay threads esperando
    if (adentro[cat] == 0) {
        if (!emptyQueue(queues[oponente])) {
            // Despertar threads de la categoria opuesta
            Request *req = (Request *)get(queues[oponente]);            // Sacar un Request de la cola, de la categoria opuesta
            pthread_cond_signal(&req->cond);                            // Despertar al thread
            adentro[oponente]++;                                        // El thread de la categoria opuesta entra al recurso
            ultimo_cat = oponente;                                      // Actualizar info sobre quien fue el ultimo en entrar
        } else if (!emptyQueue(queues[cat])) {
            // Si no hay threads de la categoria opuesta, despertar uno de la misma categoria
            Request *req = (Request *)get(queues[cat]);                 // Sacar un Request de la cola, de la misma categoria
            pthread_cond_signal(&req->cond);                            // Despertar al thread
            adentro[cat]++;                                             // El thread de la misma categoria entra al recurso
        }
    }

    pthread_mutex_unlock(&mutex);               // Salir de la zona critica
}

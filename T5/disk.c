#define _XOPEN_SOURCE 500

#include "nthread-impl.h"
#include "disk.h"

// Variables globales
static PriQueue *queueAboveCT;                          // Cola con las solicitudes de pistas >= currentTrack
static PriQueue *queueBelowCT;                          // Cola con las solicitudes de pistas < currentTrack
static int currentTrack = 0;                            // La pista actual donde esta el cabezal => currentTrack
static int diskBusy = 0;                                // Indica si el disco esta ocupado

// Funcion f
void f(nThread thisTh) {
    START_CRITICAL
    priDel(queueAboveCT, thisTh);                       // Eliminar de la cola de pistas >= currentTrack
    priDel(queueBelowCT, thisTh);                       // Eliminar de la cola de pistas < currentTrack
    thisTh->ptr = NULL;                                 // Establecer el campo ptr del thread a NULL
    END_CRITICAL
}

// Funcion para inicializar el sistema de disco
void iniDisk(void) {
    queueAboveCT = makePriQueue();                      // Inicializa la cola de prioridades para pistas >= currentTrack
    queueBelowCT = makePriQueue();                      // Inicializa la cola de prioridades para pistas < currentTrack
    currentTrack = 0;                                   // El cabezal empieza en la pista 0
    diskBusy = 0;                                       // El disco esta inicialmente libre
}

// Funcion para limpiar los recursos
void cleanDisk(void) {
    destroyPriQueue(queueAboveCT);                      // Destruye la cola de pistas >= currentTrack
    destroyPriQueue(queueBelowCT);                      // Destruye la cola de pistas < currentTrack
}

// Solicita acceso exclusivo al disco
int nRequestDisk(int track, int timeout) {
    START_CRITICAL

    if (!diskBusy) {                                    // Si el disco esta libre
        diskBusy = 1;
        currentTrack = track;
        END_CRITICAL
        return 0;                                       // El disco estaba libre, acceso inmediato
    }

    nThread thisTh = nSelf();
    int *trackPtr = malloc(sizeof(int));                // Espacio en memoria para almacenar el valor de la pista
    *trackPtr = track;                                  // Guardar el valor de track en la memoria asignada
    thisTh->ptr = trackPtr;                             // Apuntar a la direccion del valor almacenado

    if (track >= currentTrack) {
        priPut(queueAboveCT, thisTh, track);            // Colocar en la cola de pistas >= currentTrack
    } else {
        priPut(queueBelowCT, thisTh, track);            // Colocar en la cola de pistas < currentTrack
    }

    if (timeout > 0) {                                  // Si timeout >= 0
        suspend(WAIT_REQUEST_TIMEOUT);                  // Suspender el thread con el estado WAIT_REQUEST_TIMEOUT
        nth_programTimer(timeout * 1000000LL, f);  
    } else {                                            // Si timeout < 0   
        suspend(WAIT_REQUEST);                          // Suspender indefinidamente si no hay timeout
    }

    schedule();                                         // Reanudar cuando el timeout expira o el disco está disponible

    if (thisTh->ptr == NULL) {
        free(trackPtr);                                 // Liberar la memoria si no se obtuvo acceso al disco
        END_CRITICAL
        return 1;                                       // El timeout expiro y no se obtuvo acceso al disco
    }

    END_CRITICAL
    return 0;                                           // Retorna 0 porque el thread obtuvo acceso al disco
}

// Notificacion de termino de uso del disco
void nReleaseDisk() {
    START_CRITICAL

    nThread nextTh = NULL;                              // Inicializar el siguiente thread a NULL

    // Si la cola queueAboveCT tiene solicitudes pendientes
    if (!emptyPriQueue(queueAboveCT)) {
        nextTh = priGet(queueAboveCT);                  // Obtener el siguiente thread
        currentTrack = *(int *)nextTh->ptr;             // Actualizar la pista actual
    }
    // Si la cola queueAboveCT está vacía pero hay solicitudes en queueBelowCT
    else if (!emptyPriQueue(queueBelowCT)) {
        // Intercambiar las colas
        PriQueue *temp = queueAboveCT;
        queueAboveCT = queueBelowCT;
        queueBelowCT = temp;

        nextTh = priGet(queueAboveCT);                  // Obtener el siguiente thread despues de intercambiar
        currentTrack = *(int *)nextTh->ptr;             // Actualizar la pista actual
    }

    // Verificar si el hilo seleccionado sigue siendo válido
    if (nextTh != NULL && nextTh->ptr != NULL) {        // Verificar si el thread no ha sido cancelado
        if (nextTh->status == WAIT_REQUEST_TIMEOUT) {
            nth_cancelThread(nextTh);                   // Cancelar el temporizador si el thread tiene timeout
        }
        free(nextTh->ptr);                              // Liberar la memoria despues de usarla
        setReady(nextTh);                               // Marcar el thread como listo
    } else {
        // Si no se selecciono un hilo valido o el hilo fue cancelado, liberar el disco
        diskBusy = 0;
    }

    schedule();                                         // Reanudar la ejecucion de otros threads

    END_CRITICAL
}

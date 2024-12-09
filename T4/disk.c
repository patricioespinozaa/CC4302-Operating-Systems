#define _XOPEN_SOURCE 500
#include "nthread-impl.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <nthread.h>

#include "disk.h"
#include "pss.h"

// Variables globales
static PriQueue *queueAboveCT;                          // Cola con las solicitudes de pistas >= currentTrack
static PriQueue *queueBelowCT;                          // Cola con las solicitudes de pistas < currentTrack
static int currentTrack = 0;                            // La pista actual donde está el cabezal => currentTrack
static int diskBusy = 0;                                // Indica si el disco esta ocupado

// Función para inicializar el sistema de disco
void iniDisk(void) {
    queueAboveCT = makePriQueue();                      // Inicializa la cola de prioridades para pistas >= U
    queueBelowCT = makePriQueue();                      // Inicializa la cola de prioridades para pistas < U
    currentTrack = 0;                                   // El cabezal empieza en la pista 0
    diskBusy = 0;                                       // El disco está inicialmente libre
}

// Función para limpiar los recursos
void cleanDisk(void) {
    destroyPriQueue(queueAboveCT);                      // Destruye la cola de pistas >= CurrentTrack
    destroyPriQueue(queueBelowCT);                      // Destruye la cola de pistas < CurrentTrack
}

// Solicita acceso exclusivo al disco
void nRequestDisk(int track, int delay) {
    START_CRITICAL

    if (!diskBusy) {                                    // Si el disco está libre
        diskBusy = 1;                                   // Ocupar el disco
        currentTrack = track;                           // Actualizar la pista actual al track solicitado
    } else {
        // Si el disco está ocupado, agregar el thread a la cola de prioridad adecuada
        nThread thisTh = nSelf();                       // Obtener el thread actual
        thisTh->ptr = (void *)(long)track;              // Guardar la pista solicitada en el campo ptr del thread

        // Si la pista solicitada es mayor o igual a la pista actual, agregar a queueAboveCT
        if (track >= currentTrack) {
            priPut(queueAboveCT, thisTh, track);        // Insertar en la cola con prioridad
        } else {
            // Si la pista solicitada es menor que la pista actual, agregar a queueBelowCT
            priPut(queueBelowCT, thisTh, track);
        }

        suspend(WAIT_REQUEST);                          // Suspender el thread hasta que sea su turno
        schedule();                                     // Invocar el planificador para continuar la ejecución
    }

    END_CRITICAL
}

// Notificación de termino de uso del disco
void nReleaseDisk() {
    START_CRITICAL

    nThread nextTh = NULL;                               // Inicializar el siguiente thread a NULL

    // Si la cola queueAboveCT tiene solicitudes pendientes
    if (!emptyPriQueue(queueAboveCT)) {
        nextTh = priGet(queueAboveCT);                   // Obtener el siguiente thread
        currentTrack = (int)(long)nextTh->ptr;           // Actualizar la pista actual
    }
    // Si la cola queueAboveCT está vacía pero hay solicitudes en queueBelowCT
    else if (!emptyPriQueue(queueBelowCT)) {
        // Intercambiar las colas: ahora queueBelowCT será la cola de pistas >= currentTrack
        PriQueue *temp = queueAboveCT;
        queueAboveCT = queueBelowCT;
        queueBelowCT = temp;

        nextTh = priGet(queueAboveCT);                   // Obtener el siguiente thread después de intercambiar
        currentTrack = (int)(long)nextTh->ptr;           // Actualizar la pista actual
    }

    if (nextTh != NULL) {                                // Si hay un thread listo para ejecutarse
        setReady(nextTh);                                // Marcar el thread como listo para ejecutarse
    } else {
        diskBusy = 0;                                    // No hay más solicitudes pendientes, liberar el disco
    }

    schedule();                                          // Reanudar la ejecución de otros threads

    END_CRITICAL
}

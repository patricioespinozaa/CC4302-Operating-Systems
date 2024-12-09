#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>

#include "spinlocks.h"   
#include "pss.h"         
#include "subasta.h"     

enum { ABIERTA, CERRADA };                                      // Estados de la subasta

// Estructura ofertas
typedef struct Oferta {
    double precio;                                              // Precio que se oferta a la subasta
    int resultado;                                              // 1 si es aceptada, 0 si es rechazada
    int lock;                                                   // Spin-lock para esta oferta
} Oferta;

// Estructura para una subasta
struct subasta {
    int lock;                                                   // Spin-lock para la exclusión mutua de la subasta
    int n;                                                      // Número de productos disponibles
    int estado;                                                 // Estado de la subasta: ABIERTA o CERRADA
    int ofertasLock;                                            // Spin-lock para la cola de ofertas
    PriQueue *ofertas;                                          // Cola de prioridad para manejar ofertas
};

// Función para crear una nueva subasta
Subasta nuevaSubasta(int unidades) {
    Subasta s = (Subasta)malloc(sizeof(struct subasta));
    s->n = unidades;
    s->estado = ABIERTA;
    s->lock = OPEN;
    s->ofertasLock = OPEN;   
    s->ofertas = makePriQueue();  
    return s;
}

// Función para destruir la subasta y liberar los recursos
void destruirSubasta(Subasta s) {
    while (!emptyPriQueue(s->ofertas)) {                        // Liberar cada oferta en la cola
        Oferta* oferta = priGet(s->ofertas);
        free(oferta);  
    }
    destroyPriQueue(s->ofertas);                                // Liberar la cola de prioridad
    free(s);                                                    // Liberar la estructura de la subasta
}

// Función para hacer una oferta en la subasta
int ofrecer(Subasta s, double precio) {

    spinLock(&s->lock);                                         // Bloquear la subasta
    //fprintf(stderr, "Oferta de %f recibida\n", precio);

    if (s->estado == CERRADA) {                                 // La subasta está cerrada   
        spinUnlock(&s->lock);                                   // Desbloquear la subasta
        //fprintf(stderr, "La subasta está cerrada, oferta rechazada\n");
        return 0;                                               // La subasta está cerrada, no se acepta la oferta
    }

    Oferta* oferta = (Oferta*)malloc(sizeof(Oferta));
    oferta->precio = precio;
    oferta->resultado = 0;
    oferta->lock = CLOSED;

    spinLock(&s->ofertasLock);                                  // Bloquear la cola de ofertas antes de modificarla
    if (priLength(s->ofertas) < s->n) {                         // Aún hay espacio para aceptar la oferta sin rechazar ninguna
        //fprintf(stderr, "Oferta de %f aceptada\n", precio);
        priPut(s->ofertas, oferta, precio);                     // Insertar la oferta en la cola de prioridad
    } else {
        // La subasta está llena, revisar la peor oferta
        Oferta* peorOferta = priPeek(s->ofertas);               // Obtener la peor oferta
        if (peorOferta->precio < precio) {
            //fprintf(stderr, "Sustituyendo la peor oferta de %f con una nueva oferta de %f\n", peorOferta->precio, precio);
            priGet(s->ofertas);                                 // Retirar la peor oferta
            peorOferta->resultado = 0;                          // Marcarla como rechazada
            spinUnlock(&peorOferta->lock);                      // Desbloquear la peor oferta
            free(peorOferta); 
            priPut(s->ofertas, oferta, precio);                 // Insertar la nueva oferta
        } else {
            //fprintf(stderr, "Oferta de %f rechazada por ser inferior a la peor oferta de %f\n", precio, peorOferta->precio);
            free(oferta);                                       // Liberar memoria de la oferta
            spinUnlock(&s->ofertasLock);                        // Desbloquear la cola de ofertas
            spinUnlock(&s->lock);                               // Desbloquear la subasta
            return 0;                                           // Se rechaza
        }
    }

    spinUnlock(&s->ofertasLock);                                // Desbloquear la cola de ofertas
    spinUnlock(&s->lock);                                       // Desbloquear la subasta
    spinLock(&oferta->lock);                                    // Esperar el resultado de la oferta
    int resultado = oferta->resultado;                          // Obtener el resultado de la oferta
    //fprintf(stderr, "Oferta de %f %s\n", precio, resultado == 1 ? "aceptada" : "rechazada");
    spinUnlock(&oferta->lock);                                  // Desbloquear la oferta
    return resultado;
}

// Función para adjudicar la subasta
double adjudicar(Subasta s, int *punidades) {
    spinLock(&s->lock);                                         // Bloquear la subasta

    s->estado = CERRADA;                                        // Cerrar la subasta
    double recaudacion = 0;                                     // Inicializar la recaudación
    int vendidos = 0;                                           // Inicializar el número de productos vendidos
    *punidades = s->n;                                          // Inicializar el número de unidades

    spinLock(&s->ofertasLock);                                  // Bloquear la cola de ofertas antes de acceder a ella
    while (*punidades > 0 && !emptyPriQueue(s->ofertas)) {      // Mientras haya unidades y ofertas
        Oferta* oferta = priGet(s->ofertas);                    // Obtener la mejor oferta
        recaudacion += oferta->precio;                          // Sumar el precio de la oferta a la recaudación
        oferta->resultado = 1;                                  // Marcar oferta como aceptada
        vendidos++;                                             // Incrementar el número de productos vendidos
        //fprintf(stderr, "Oferta de %f aceptada, recaudación total: %f\n", oferta->precio, recaudacion);
        spinUnlock(&oferta->lock);                              // Desbloquear la oferta aceptada
        free(oferta); 
    }

    // Rechazar las ofertas restantes no adjudicadas
    while (!emptyPriQueue(s->ofertas)) {
        Oferta* oferta = priGet(s->ofertas);                    // Obtener la siguiente oferta
        oferta->resultado = 0;                                  // Marcar oferta como rechazada
        //fprintf(stderr, "Oferta de %f rechazada\n", oferta->precio);
        spinUnlock(&oferta->lock);                              // Desbloquear la oferta rechazada
        free(oferta);
    }

    //fprintf(stderr, "Subasta adjudicada con recaudación=%f\n", recaudacion);
    *punidades = s->n - vendidos;                               // Unidades no vendidas
    spinUnlock(&s->ofertasLock);                                // Desbloquear la cola de ofertas
    spinUnlock(&s->lock);                                       // Desbloquear la subasta
    return recaudacion;
}
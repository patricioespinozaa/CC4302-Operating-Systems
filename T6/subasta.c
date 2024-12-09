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
    spinLock(&s->lock);
    while (!emptyPriQueue(s->ofertas)) {                        // Liberar cada oferta en la cola
        Oferta* oferta = priGet(s->ofertas);
        free(oferta);  
    }
    destroyPriQueue(s->ofertas);                                // Liberar la cola de prioridad
    spinUnlock(&s->lock);
    free(s);                                                    // Liberar la estructura de la subasta
}

// Función para hacer una oferta en la subasta
int ofrecer(Subasta s, double precio) {

    spinLock(&s->lock);                                         // Bloquear la subasta

    Oferta* oferta = (Oferta*)malloc(sizeof(Oferta));
    oferta->precio = precio;
    oferta->resultado = 0;
    oferta->lock = CLOSED;

    spinLock(&s->ofertasLock);                                  // Bloquear la cola de ofertas antes de modificarla
    if (priLength(s->ofertas) < s->n) {                         // Aún hay espacio para aceptar la oferta sin rechazar ninguna
        priPut(s->ofertas, oferta, precio);                     // Insertar la oferta en la cola de prioridad
    } else {
        // La subasta está llena, revisar la peor oferta
        Oferta* peorOferta = priPeek(s->ofertas);               // Obtener la peor oferta
        if (peorOferta->precio < precio) {
            priGet(s->ofertas);                                 // Retirar la peor oferta
            peorOferta->resultado = 0;                          // Marcarla como rechazada
            spinUnlock(&peorOferta->lock);                      // Desbloquear la peor oferta
            priPut(s->ofertas, oferta, precio);
        } else {
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
    spinUnlock(&oferta->lock);                                  // Desbloquear la oferta
    free(oferta); 
    return resultado;
}

// Función para adjudicar la subasta
double adjudicar(Subasta s, int *punidades) {
    spinLock(&s->lock);                                         // Bloquear la subasta

    s->estado = CERRADA;                                        // Cerrar la subasta
    double recaudacion = 0;                                     // Inicializar la recaudación
    int vendidos = 0;                                           // Inicializar el número de productos vendidos

    spinLock(&s->ofertasLock);                                  // Bloquear la cola de ofertas antes de acceder a ella
    //while (vendidos < s->n && !emptyPriQueue(s->ofertas)) {      // Mientras haya unidades y ofertas
    //    Oferta* oferta = priGet(s->ofertas);                    // Obtener la mejor oferta
    //    recaudacion += oferta->precio;                          // Sumar el precio de la oferta a la recaudación
    //    oferta->resultado = 1;                                  // Marcar oferta como aceptada
    //    vendidos++;                                             // Incrementar el número de productos vendidos
    //    spinUnlock(&oferta->lock);                              // Desbloquear la oferta aceptada
    //}

    while(!emptyPriQueue(s->ofertas)){
        Oferta* oferta = priGet(s->ofertas);
        if(vendidos < s->n){
            recaudacion += oferta -> precio;
            oferta -> resultado = 1;
            vendidos++;
            spinUnlock(&oferta->lock);
        } else {
            oferta -> resultado = 0;
            spinUnlock(&oferta->lock);
        }
    }
    // Rechazar las ofertas restantes no adjudicadas

    *punidades = s->n - vendidos;                               // Unidades no vendidas
    spinUnlock(&s->ofertasLock);                                // Desbloquear la cola de ofertas
    spinUnlock(&s->lock);                                       // Desbloquear la subasta
    return recaudacion;
}

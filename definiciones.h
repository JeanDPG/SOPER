#ifndef DEFINICIONES_H
#define DEFINICIONES_H

#include <pthread.h>

// Estructura para mensajes entre minero y registrador
typedef struct {
    int target;
    int solution;
    int ronda;
} Message;

//Estructura para pasar a los hilos de búsqueda de POW
typedef struct {
    int start;
    int stop;
    int target;
    int* found;
} Rank;

#endif

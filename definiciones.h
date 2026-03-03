#ifndef DEFINICIONES_H
#define DEFINICIONES_H

#include <pthread.h>

typedef struct {
    int target;
    int solution;
    int ronda;
} Message;

typedef struct {
    int start;
    int stop;
    int target;
    int* found;
    pthread_mutex_t* mutex;
} Rank;

#endif

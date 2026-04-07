#ifndef MINER_H
#define MINER_H
#include <semaphore.h>

void ejecutar_minero(int n_secs, int n_threads, int pipe_escritura, int pipe_lectura);

void escritura_de_ficheroPIDS(pid_t pid/*, int* ganador, sem_t* sem */);
void escribirTarget(pid_t pid/*, int init_target, sem_t* sem*/);
void borrado_de_ficheros(pid_t pid_to_delete/*, sem_t* sem*/);
#endif

#ifndef MINER_H
#define MINER_H

#include <semaphore.h>
#include <mqueue.h>
#include "definiciones.h"

void ejecutar_minero(int n_secs, int n_threads,
                     int pipe_escritura, int pipe_lectura,
                     ShmSistema *shm, mqd_t mq);

#endif
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include "minero.h"
#include "definiciones.h"
#include "pow.h"

void* powRank(void* arg) {
    Rank* r = (Rank*) arg;
    for (int i = r->start; i < r->stop; i++) {
        if (*(r->found) != -1) break;
        if (pow_hash(i) == r->target) {
            pthread_mutex_lock(r->mutex);
            if (*(r->found) == -1) *(r->found) = i;
            pthread_mutex_unlock(r->mutex);
            break;
        }
    }
    free(r);
    return NULL;
}

void ejecutar_minero(int target_ini, int rounds, int n_threads, int pipe_escritura, int pipe_lectura) {
    int target = target_ini;
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

    for (int i = 0; i < rounds; i++) {
        pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
        int found = -1;
        int range_size = POW_LIMIT / n_threads;

        for (int j = 0; j < n_threads; j++) {
            Rank* r = malloc(sizeof(Rank));
            r->start = j * range_size;
            r->stop = (j == n_threads - 1) ? POW_LIMIT : (j + 1) * range_size;
            r->target = target;
            r->found = &found;
            r->mutex = &mutex;
            pthread_create(&threads[j], NULL, powRank, r);
        }

        for (int j = 0; j < n_threads; j++) pthread_join(threads[j], NULL);
        free(threads);

        if (found != -1) {
            printf("Solution accepted : %08d --> %08d\n", target, found);
            Message msg = {target, found, i};
            write(pipe_escritura, &msg, sizeof(Message));
            
            char ack;
            read(pipe_lectura, &ack, sizeof(char));
            target = found;
        } else break;
    }

    Message end_msg = {0, -1, 0};
    write(pipe_escritura, &end_msg, sizeof(Message));
}

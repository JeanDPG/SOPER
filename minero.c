#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include "minero.h"
#include "definiciones.h"
#include "pow.h"

/* --------------------------------------------------------------------------
 * FUNCIÓN (Hilo): powRank
 * --------------------------------------------------------------------------
 * Búsqueda de POW en rango asignado.
 * * PARÁMETROS:
 * [in/out] arg : Puntero a estructura 'Rank' (rangos y puntero a solución).
 * *SALIDA:
 * 1. Si detecta solución externa (*found != -1) -> libera y retorna.
 * 2. Si halla solución local (pow_hash == target) -> registra, libera y retorna.
 * 3. Si agota su rango -> libera y retorna.
 * -------------------------------------------------------------------------- */
void* powRank(void* arg) {
    Rank* r = (Rank*) arg;

    for (int i = r->start; i < r->stop; i++) {
        if (*(r->found) != -1) {
            free(r); 
            return NULL;
        }

        if (pow_hash(i) == r->target) {
            if (*(r->found) == -1) {
                *(r->found) = i;
            }
            free(r);
            return NULL;
        }
    }
    free(r);
    return NULL;
}


/* --------------------------------------------------------------------------
 * FUNCIÓN: ejecutar_minero
 * --------------------------------------------------------------------------
 * PROPÓSITO:
 * Organiza el proceso de minería por rondas utilizando multihilo.
 * * PARÁMETROS:
 * [in] target_ini     : Hash inicial a resolver.
 * [in] rounds         : Número máximo de iteraciones de minería.
 * [in] n_threads      : Cantidad de hilos de ejecución paralela.
 * [in] pipe_escritura : FD para enviar mensajes al registrador.
 * [in] pipe_lectura   : FD para recibir el ACK del registrador.
 * * LÓGICA:
 * 1. Divide el espacio de búsqueda (POW_LIMIT) entre n_threads.
 * 2. Lanza hilos 'powRank' y espera su finalización (join).
 * 3. Si halla solución: Envía Message, bloquea por ACK y actualiza target.
 * 4. Si falla: Termina el bucle de rondas.
 * 5. Al finalizar: Envía mensaje (solution = -1).
 * -------------------------------------------------------------------------- */
void ejecutar_minero(int target_ini, int rounds, int n_threads, int pipe_escritura, int pipe_lectura) {
    int target = target_ini;

    for (int i = 0; i < rounds; i++) {
        pthread_t *threads = malloc(n_threads * sizeof(pthread_t));

        // Variable compartida donde los hilos guardan la solución encontrada
        int found = -1;
        
        int range_size = POW_LIMIT / n_threads;
        int count = 0;

        for (int j = 0; j < n_threads; j++) {
            Rank* r = malloc(sizeof(Rank));
            r->start = count;
            count += range_size;
            r->stop = count;
            r->target = target;
            r->found = &found;
            if(pthread_create(&threads[j], NULL, powRank, r)){
                for (int i = 0; i < j; i++) pthread_join(threads[i], NULL);
            }


        }

        // Espera a que todos los hilos terminen antes de continuar
        for (int j = 0; j < n_threads; j++) pthread_join(threads[j], NULL);
        free(threads);

        if (found != -1) {
            printf("Solution accepted : %08d --> %08d\n", target, found);
            Message msg = {target, found, i};
            // Envio de la solución al registrador mediante el pipe
            write(pipe_escritura, &msg, sizeof(Message));
            
            char ack;
            // Espera de confirmación (ACK) antes de comenzar la siguiente ronda
            read(pipe_lectura, &ack, sizeof(char));
            
            // La solución encontrada pasa a ser el nuevo target
            target = found;
        } else break;
    }

    Message end_msg = {0, -1, 0};
    write(pipe_escritura, &end_msg, sizeof(Message));
}

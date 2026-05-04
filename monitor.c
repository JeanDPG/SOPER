/**
 * @file monitor.c
 * @brief Proceso Monitor y validador central del sistema de minería.
 * 
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <semaphore.h>
#include <mqueue.h>

#include "definiciones.h"
#include "pow.h"

/* Variables globales para permitir la limpieza en el manejador de señales */
ShmSistema *mem = NULL;
mqd_t cola = -1;

/**
 * @brief Manejador de la señal SIGINT (Ctrl+C).
 * Desvincula la cola de mensajes y la memoria compartida antes de salir abruptamente.
 * @param sig Número de la señal.
 */
void cerrar_todo(int sig) {
    mq_unlink(MQ_NAME);
    shm_unlink(SHM_SISTEMA);
    exit(0);
}

/**
 * @brief Punto de entrada principal del Monitor.
 * @param argc Cantidad de argumentos (debe ser 3).
 * @param argv[1] Latencia l1 (en milisegundos) para el hilo productor (Padre).
 * @param argv[2] Latencia l2 (en milisegundos) para el hilo consumidor (Hijo).
 */
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <latencia_productor_ms> <latencia_consumidor_ms>\n", argv[0]);
        return 1;
    }
    int l1 = atoi(argv[1]);
    int l2 = atoi(argv[2]);

    /* Eliminar recursos IPC previos en caso de que existieran por una mala finalización */
    shm_unlink(SHM_SISTEMA);
    mq_unlink(MQ_NAME);

     /* Inicialización de la Memoria */

    int f_shm = shm_open(SHM_SISTEMA, O_RDWR | O_CREAT | O_EXCL, 0666);
    ftruncate(f_shm, sizeof(ShmSistema));
    mem = mmap(NULL, sizeof(ShmSistema), PROT_READ | PROT_WRITE, MAP_SHARED, f_shm, 0);
    close(f_shm);

    mem->in = 0;
    mem->out = 0;

    /* Inicialización de los semáforos para el buffer circular (Productor-Consumidor) y el sistema */
    sem_init(&mem->sem_sistema, 1, 1);
    sem_init(&mem->s_empty, 1, BUFFER_SIZE);
    sem_init(&mem->s_fill, 1, 0);
    sem_init(&mem->s_mtx, 1, 1);

     /* Inicialización de la Cola de Mensajes*/
    struct mq_attr attr;
    attr.mq_flags = 0;
    attr.mq_maxmsg = MQ_MAX_MSG;
    attr.mq_msgsize = sizeof(Bloque);
    attr.mq_curmsgs = 0;

    cola = mq_open(MQ_NAME, O_RDWR | O_CREAT | O_EXCL, 0666, &attr);

    /* Configuración del manejador de señales para limpieza segura */
    struct sigaction sa;
    sa.sa_handler = cerrar_todo;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    pid_t p = fork();

    if (p == 0) {
         /* PROCESO HIJO (Consumidor)
          Lee bloques del buffer circular en memoria compartida e imprime.*/

        printf("[%d] Printing blocks...\n", getpid());
        Bloque b;
        while (1) {
            /* Sincronización para consumir del buffer circular */
            sem_wait(&mem->s_fill);
            sem_wait(&mem->s_mtx);
            b = mem->b[mem->out];
            mem->out = (mem->out + 1) % BUFFER_SIZE;
            sem_post(&mem->s_mtx);
            sem_post(&mem->s_empty);

            /* Salir si se recibe un bloque con la bandera de finalizar */
            if (b.finalizar) break;

            if (b.valida) {
                printf("Solution accepted: %08ld --> %08ld\n", b.target, b.solution);
            } else {
                printf("Solution rejected: %08ld !-> %08ld\n", b.target, b.solution);
            }
            fflush(stdout);
            usleep(l2 * 1000);
        }
        printf("[%d] Finishing\n", getpid());
        exit(0);
    } else {
         /* PROCESO PADRE (Productor)
           Escucha la cola de mensajes, valida bloques y los pasa al buffer.*/
        Bloque b2;
        while (1) {
            mq_receive(cola, (char *)&b2, sizeof(Bloque), NULL);
            if (!b2.finalizar) {
                /* Validar matemáticamente el bloque entrante contra target */
                b2.valida = (pow_hash((int)b2.solution) == (int)b2.target) ? 1 : 0;
            }
            
            /* Sincronización para producir en el buffer circular */
            sem_wait(&mem->s_empty);
            sem_wait(&mem->s_mtx);
            mem->b[mem->in] = b2;
            mem->in = (mem->in + 1) % BUFFER_SIZE;
            sem_post(&mem->s_mtx);
            sem_post(&mem->s_fill);

            if (b2.finalizar) break;
            usleep(l1 * 1000);
        }
        wait(NULL);
    }

    /* =========================================================================
     * Limpieza y Destrucción de Recursos (solo alcanzado por el padre al final)
     * ========================================================================= */
    sem_destroy(&mem->sem_sistema);
    munmap(mem, sizeof(ShmSistema));
    shm_unlink(SHM_SISTEMA);
    mq_close(cola);
    mq_unlink(MQ_NAME);

    return 0;
}
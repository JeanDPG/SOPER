#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "registrador.h"
#include "definiciones.h"
#include "pow.h"

/* --------------------------------------------------------------------------
 * FUNCIÓN: ejecutar_registrador
 * --------------------------------------------------------------------------
 * Valida y registra en disco las soluciones de minería recibidas.
 *
 * FLUJO:
 * 1. Recibe 'Message' del minero (bloqueante).
 * 2. Valida PoW localmente con pow_hash.
 * 3. Escribe el bloque en "miner_[ppid].txt".
 * 4. Envía ACK ('K') para liberar al minero hacia la siguiente ronda.
 *
 * PARÁMETROS:
 * [in] pipe_lectura   : FD para recibir estructuras 'Message'.
 * [in] pipe_escritura : FD para enviar ACK al minero.
 * [in] ppid           : PID del proceso minero padre (nombre del log).
 * -------------------------------------------------------------------------- */
void ejecutar_registrador(int pipe_lectura, int pipe_escritura, pid_t ppid) {
    char filename[64];
    snprintf(filename, sizeof(filename), "miner_%d.txt", ppid);

    int fd_log = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_log == -1) { perror("open log"); exit(EXIT_FAILURE); }

    Message msg;
    char ack = 'K';

    while (read(pipe_lectura, &msg, sizeof(Message)) > 0) {

        // solution == -1 es la señal de terminación del minero
        if (msg.solution == -1) break;

        //printf("Registrador recibió solución %d\n", msg.solution);

        const char *status = (pow_hash(msg.solution) <= msg.target)
                             ? "validated" : "rejected";

        dprintf(fd_log, "Round    : [ %d ]\n",          msg.ronda);
        dprintf(fd_log, "Winner   : [ %d ]\n",          ppid);
        dprintf(fd_log, "Target   : [ %d ]\n",          msg.target);
        dprintf(fd_log, "Solution : %08d ( %s )\n\n",   msg.solution, status);

        // ACK al minero: el bloque fue procesado, puede continuar
        if (write(pipe_escritura, &ack, sizeof(char)) == -1) {
            perror("write ack");
            break;
        }
    }

    close(fd_log);
    close(pipe_lectura);
    close(pipe_escritura);
    exit(EXIT_SUCCESS);
}
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
 * Validar y registrar en disco las soluciones de minería recibidas.
 * * FLUJO:
 * 1. Recibe 'Message' del minero (Bloqueante).
 * 2. Valida PoW localmente (pow_hash).
 * 3. Escribe resultados en log "miner_[ppid].txt".
 * 4. Envía ACK ('K') para liberar al minero hacia la siguiente ronda.
 * * PARÁMETROS:
 * [in] pipe_lectura   : FD para recibir estructuras 'Message'.
 * [in] pipe_escritura  : FD para enviar confirmación (ACK) al minero.
 * [in] ppid           : PID del proceso minero (para nombre de log).
 * * SALIDA:
 * - Finaliza con EXIT_SUCCESS al recibir solución -1.
 * - Finaliza con EXIT_FAILURE si falla la apertura del archivo.
 * -------------------------------------------------------------------------- */
void ejecutar_registrador(int pipe_lectura, int pipe_escritura, pid_t ppid) {
    char filename[64];
    sprintf(filename, "miner_%d.txt", ppid);
    
    int fd_log = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_log == -1) exit(EXIT_FAILURE);

    Message msg;
    // ACK para enviar al minero como confirmacionde recepcion
    char ack = 'K';

    // Lectura continua desde el pipe hasta que el emisor lo cierre
    while (read(pipe_lectura, &msg, sizeof(Message)) > 0) {
        if (msg.solution == -1) return;

        const char* status = (pow_hash(msg.solution) == msg.target) ? "validated" : "rejected";

        dprintf(fd_log, "Id : [ %d ]\n", msg.ronda);
        dprintf(fd_log, "Winner : [ %d ]\n", ppid);
        dprintf(fd_log, "Target : [ %d ]\n", msg.target);
        dprintf(fd_log, "Solution : %08d ( %s )\n", msg.solution, status);
        dprintf(fd_log, "Votes : [ %d ]/[ %d ]\n", msg.ronda, msg.ronda);
        dprintf(fd_log, "Wallets : [ %d ]:[ %d ]\n\n", ppid, msg.ronda);

        // Confirmación al proceso minero de que el bloque fue procesado
        write(pipe_escritura, &ack, sizeof(char));
    }

    close(fd_log);
    exit(EXIT_SUCCESS);
}

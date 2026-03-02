#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include "registrador.h"
#include "definiciones.h"
#include "pow.h"

void ejecutar_registrador(int pipe_lectura, int pipe_escritura, pid_t ppid) {
    char filename[64];
    sprintf(filename, "miner_%d.txt", ppid);
    
    int fd_log = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_log == -1) exit(EXIT_FAILURE);

    Message msg;
    char ack = 'K';

    while (read(pipe_lectura, &msg, sizeof(Message)) > 0) {
        if (msg.solution == -1) break;

        const char* status = (pow_hash(msg.solution) == msg.target) ? "validated" : "rejected";

        dprintf(fd_log, "Id : [ %d ]\n", msg.ronda);
        dprintf(fd_log, "Winner : [ %d ]\n", ppid);
        dprintf(fd_log, "Target : [ %d ]\n", msg.target);
        dprintf(fd_log, "Solution : %08d ( %s )\n", msg.solution, status);
        dprintf(fd_log, "Votes : [ %d ]/[ %d ]\n", msg.ronda, msg.ronda);
        dprintf(fd_log, "Wallets : [ %d ]:[ %d ]\n\n", ppid, msg.ronda);
        
        write(pipe_escritura, &ack, sizeof(char));
    }

    close(fd_log);
    exit(EXIT_SUCCESS);
}

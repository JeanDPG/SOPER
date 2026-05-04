#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <semaphore.h>

#include "registrador.h"
#include "definiciones.h"
#include "pow.h"

void hacer_registro(int p_lec, int p_esc, pid_t papa, ShmSistema *memoria) {
    char arch[64];
    sprintf(arch, "%d.txt", papa);

    int fd = open(arch, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { exit(1); }

    Message m;
    char k = 'K';

    while (read(p_lec, &m, sizeof(Message)) > 0) {

        if (m.solution == -1) break;

        char *estado;
        if(pow_hash(m.solution) == m.target) {
            estado = "validated";
        } else {
            estado = "rejected";
        }

        dprintf(fd, "Id : %d\n", m.monedas_ganador);
        dprintf(fd, "Winner : %d\n", papa);
        dprintf(fd, "Target : %d\n", m.target);
        dprintf(fd, "Solution : %d ( %s )\n", m.solution, estado);
        dprintf(fd, "Votes : %d/%d\n", m.votos_aceptados, m.votos_totales);

        dprintf(fd, "Wallets :");
        sem_wait(&memoria->sem_sistema);
        for (int i = 0; i < memoria->n_carteras; i++) {
            dprintf(fd, " %d:%d", memoria->pids_carteras[i], memoria->monedas[i]);
        }
        sem_post(&memoria->sem_sistema);
        dprintf(fd, "\n\n");

        write(p_esc, &k, 1);
    }

    close(fd);
    close(p_lec);
    close(p_esc);
    exit(0);
}
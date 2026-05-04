#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <semaphore.h>
#include <mqueue.h>
#include <errno.h>

#include "minero.h"
#include "registrador.h"
#include "definiciones.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        printf("Error args\n");
        return 1;
    }

    int s = atoi(argv[1]);
    int t = atoi(argv[2]);

    int f_shm = shm_open(SHM_SISTEMA, O_RDWR, 0);
    if (f_shm == -1) {
        printf("Monitor no esta\n");
        return 1;
    }

    ShmSistema *shm_ptr = mmap(NULL, sizeof(ShmSistema), PROT_READ | PROT_WRITE, MAP_SHARED, f_shm, 0);
    close(f_shm);

    mqd_t q = mq_open(MQ_NAME, O_WRONLY);
    if (q == -1) {
        return 1;
    }

    int p1[2], p2[2];
    pipe(p1);
    pipe(p2);

    pid_t papa = getpid();
    pid_t p = fork();

    if (p == 0) {
        close(p1[1]);
        close(p2[0]);
        mq_close(q);

        hacer_registro(p1[0], p2[1], papa, shm_ptr);

        munmap(shm_ptr, sizeof(ShmSistema));
        exit(0);

    } else {
        close(p1[0]);
        close(p2[1]);

        ejecutar_minero(s, t, p1[1], p2[0], shm_ptr, q);

        close(p1[1]);
        close(p2[0]);

        wait(NULL);

        mq_close(q);
        munmap(shm_ptr, sizeof(ShmSistema));
        exit(0);
    }

    return 0;
}
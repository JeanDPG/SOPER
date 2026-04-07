#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include <semaphore.h>  // FIX: necesario para sem_unlink
#include "minero.h"
#include "registrador.h"

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <n_secs> <n_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int status;
    int n_secs    = atoi(argv[1]);
    int n_threads = atoi(argv[2]);

  
    //
    // Si tras un crash el semáforo queda bloqueado, limpiarlo manualmente:
    //   python3 -c "import ctypes; ..." o simplemente relanzar tras reiniciar.

    // Dos pipes para comunicación bidireccional:
    // p_mtor[]: minero(padre) → registrador(hijo)
    // p_rtom[]: registrador(hijo) → minero(padre)  [ACK]
    int p_mtor[2], p_rtom[2];
    if (pipe(p_mtor) == -1 || pipe(p_rtom) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    pid_t parent_pid = getpid();
    pid_t pid = fork();

    if (pid == 0) {
        // --- Proceso hijo: REGISTRADOR ---
        // Cierra extremos no utilizados por el registrador
        close(p_mtor[1]);  // no escribe hacia sí mismo
        close(p_rtom[0]);  // no lee su propio ACK

        // FIX: descomentar — sin esto el minero se bloquea esperando el ACK
        ejecutar_registrador(p_mtor[0], p_rtom[1], parent_pid);

        // ejecutar_registrador hace exit() internamente, pero por seguridad:
        exit(EXIT_SUCCESS);

    } else if (pid > 0) {
        // --- Proceso padre: MINERO ---
        // Cierra extremos no utilizados por el minero
        close(p_mtor[0]);  // no lee su propio mensaje
        close(p_rtom[1]);  // no escribe su propio ACK

        ejecutar_minero(n_secs, n_threads, p_mtor[1], p_rtom[0]);

        
        close(p_mtor[1]);
        close(p_rtom[0]);

        // Espera a que el registrador (hijo) termine
        wait(&status);
        exit(EXIT_SUCCESS);

    } else {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    return 0;
}
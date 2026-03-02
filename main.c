#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include "minero.h"
#include "registrador.h"

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <target_ini> <rounds> <n_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int status;
    int p_mtor[2], p_rtom[2];
    if (pipe(p_mtor) == -1 || pipe(p_rtom) == -1) exit(EXIT_FAILURE);
    
    pid_t parent_pid = getpid();
    pid_t pid = fork();

    if (pid == 0) {
        close(p_mtor[1]); 
        close(p_rtom[0]);
        ejecutar_registrador(p_mtor[0], p_rtom[1], parent_pid);
    
        
       
    } else {
        close(p_mtor[0]); 
        close(p_rtom[1]);
        ejecutar_minero(atoi(argv[1]), atoi(argv[2]), atoi(argv[3]), p_mtor[1], p_rtom[0]);
        
        close(p_mtor[1]); 
        close(p_rtom[0]);
    
        wait(&status2);
        printf("Minner exited with status %d\n", WEXITSTATUS(status));
        
    }
    return EXIT_SUCCESS;
}

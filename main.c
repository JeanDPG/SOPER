#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <time.h>
#include "minero.h"
#include "registrador.h"

int main(int argc, char *argv[]) {
    //clock_t inicio = clock();
    if (argc < 4) {
        fprintf(stderr, "Uso: %s <target_ini> <rounds> <n_threads>\n", argv[0]);
        return EXIT_FAILURE;
    }
    int status;
    int target_ini = atoi(argv[1]);
    int rounds = atoi(argv[2]);
    int n_threads = atoi(argv[3]);
    // Se crean dos pipes para comunicación bidireccional:
    // p_mtor: minero → registrador
    // p_rtom: registrador → minero (ACK)
    int p_mtor[2], p_rtom[2];
    if (pipe(p_mtor) == -1 || pipe(p_rtom) == -1) exit(EXIT_FAILURE);
    printf("El valor de p_mtor[0] (lectura) es: %d\n", p_mtor[0]);
    printf("El valor de p_mtor[1] (escritura) es: %d\n", p_mtor[1]);

    printf("El valor de p_rtom[0] (lectura) es: %d\n", p_rtom[0]);
    printf("El valor de p_rtom[1] (escritura) es: %d\n", p_rtom[1]);
    pid_t parent_pid = getpid();
    pid_t pid = fork();
    printf("Pid: %d\n",pid);
    if (pid == 0) {
        // Cierra extremos no utilizados
        close(p_mtor[1]); 
        close(p_rtom[0]);
        ejecutar_registrador(p_mtor[0], p_rtom[1], parent_pid);

       
    } else if (pid > 0){
        
        close(p_mtor[0]); 
        close(p_rtom[1]);
        ejecutar_minero(target_ini, rounds, n_threads, p_mtor[1], p_rtom[0]);
        
        // Cierra los pipes tras finalizar la minería
        close(p_mtor[1]); 
        close(p_rtom[0]);
        printf("Soy el minero(padre)\n");
        // Espera a que el registrador termine
        wait(&status);
        printf("Minner exited with status %d\n", WEXITSTATUS(status));
        
    }else{
        perror ( "fork" ) ;
        exit (EXIT_FAILURE);
    }
   // clock_t fin = clock();
    //double tiempo_ejecucion = (double)(fin - inicio) / CLOCKS_PER_SEC;
  //  printf("Tiempo de ejecución: %.2f segundos\n", tiempo_ejecucion);
    return EXIT_SUCCESS;
}

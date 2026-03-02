/**
 * @file:miner_a.c
 * @author: Anthony Eduardo Alvarado
 * @date:13-02-2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include "pow.h"
#define TAM 256

/**
 * @brief Ejecuta el proceso de registrador
 * @author Anthony Eduardo ALvarado Carbajal
 * @param pipe_fd array de extremos de tubería
 */
void ejecutar_registrador(int pipe_fd[2]) {

    close(pipe_fd[1]);
    
    printf("[Registrador] Proceso iniciado (PID: %d)\n", getpid());
    printf("[Registrador] Esperando datos del minero...\n");
    
    sleep(2);
    
    char buffer[TAM];
    ssize_t bytes = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[Registrador] Recibido: %s\n", buffer);
    } else if (bytes == 0) {
        printf("[Registrador] Tubería cerrada (EOF)\n");
    } else {
        perror("[Registrador] read");
    }
    
    printf("[Registrador] Terminando.\n");
    
    close(pipe_fd[0]);
    exit(EXIT_SUCCESS);
}

/**
 * @brief EJecuta el proceso minero
 * @author Anthony Eduardo Alvarado Carbajal
 * 
 * @param pipe_fd Array de dos enteros 
 * @param target_ini Objetivo inicial
 * @param rounds Número de rondas 
 * @param n_threads Número de hilos 
 * @param pid_hijo PID del proceso hijo( registrador)
 */
void ejecutar_minero(int pipe_fd[2], long int target_ini, int rounds, int n_threads, pid_t pid_hijo) {

    close(pipe_fd[0]);
    
    printf("[Minero] Proceso iniciado (PID: %d)\n", getpid());
    printf("[Minero] Parámetros: target=%ld, rondas=%d, hilos=%d\n",target_ini, rounds, n_threads);
    
    sleep(1);
    
    close(pipe_fd[1]);
    
    int status;
    waitpid(pid_hijo, &status, 0);
    
    if (WIFEXITED(status)) {
        printf("Logger exited with status %d\n", WEXITSTATUS(status));
    } else {
        printf("Logger exited unexpectedly\n");
    }
    
    printf("[Minero] Terminando.\n");
    exit(EXIT_SUCCESS);
}


int main(int argc, char *argv[]) {
    //Control de argumentos
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <TARGET_INI> <ROUNDS> <N_THREADS>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 0 5 4\n", argv[0]);
        exit(EXIT_FAILURE);
    }
    
    long int target_ini = atol(argv[1]);
    int rounds = atoi(argv[2]);
    int n_threads = atoi(argv[3]);
    
    //Control de errores
    if (rounds <= 0) {
        fprintf(stderr, "ERROR: ROUNDS debe ser positivo\n");
        exit(EXIT_FAILURE);
    }
    if (n_threads <= 0) {
        fprintf(stderr, "ERROR: N_THREADS debe ser positivo\n");
        exit(EXIT_FAILURE);
    }
    
    //Crea la tuberia
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    //Crea un proceso hijo
    pid_t pid = fork();
    
    //Ejecuta los procesos padre e hijo
    if (pid < 0) {
        perror("fork");
        exit(EXIT_FAILURE);
    } else if (pid == 0) {
        ejecutar_registrador(pipe_fd);
    } else {
        ejecutar_minero(pipe_fd, target_ini, rounds, n_threads, pid);
    }
    
    return EXIT_SUCCESS;
}
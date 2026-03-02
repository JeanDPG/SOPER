/**
 * @file:miner_b.c
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
#include <pthread.h>
#include "pow.h"
#define TAM 256

/**
 * @brief Estructura para pasarle los datos a cada hilo
 */
typedef struct {
    long int target;        
    long int start;         
    long int end;           
    long int *solution;     
    int *found;             
    pthread_mutex_t *mutex; 
} thread_args_t;

/**
 * @brief Ejecuta cada hilo
 * @author Anthony Eduardo Alvarado Carbajal
 * @param arg puntero a la estructura
 * @return NULL
 */
void *miner_thread(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    
    // Recorre su rango del hilo
    for (long int i = args->start; i < args->end; i++) {
        
        // Comprueba si algun otro hilo tiene solucion
        pthread_mutex_lock(args->mutex);
        int solution_found = *(args->found);
        pthread_mutex_unlock(args->mutex);
        
        // Si existe una solución se retira el hilo
        if (solution_found) {
            pthread_exit(NULL);
        }
        
        // Prueba el número actual
        if (pow_hash(i) == args->target) {
        
            pthread_mutex_lock(args->mutex);
            if (!*(args->found)) {  
                *(args->found) = 1;      
                *(args->solution) = i;    
            }
            pthread_mutex_unlock(args->mutex);
            
            pthread_exit(NULL); 
        }
    }
    
    pthread_exit(NULL);
}

/**
 * @brief Ejecuta el proceso registrador 
 * @author Anthony Eduardo Alvarado Carbajal
 * @param pipe_fd: Los extremos de la tubería
 */
void ejecutar_registrador(int pipe_fd[2]) {
    
    close(pipe_fd[1]); 
    
    printf("[Registrador] Proceso iniciado (PID: %d)\n", getpid());
    printf("[Registrador] Esperando datos del minero...\n");
    
    sleep(2);
    
    // Miramos si el minero nos ha mandado algún mensaje
    char buffer[TAM];
    ssize_t bytes = read(pipe_fd[0], buffer, sizeof(buffer) - 1);
    if (bytes > 0) {
        buffer[bytes] = '\0';
        printf("[Registrador] Recibido: %s\n", buffer);
    }
    
    printf("[Registrador] Terminando.\n");
    close(pipe_fd[0]);
    exit(EXIT_SUCCESS);
}

/**
 * @brief Ejecuta el proceso minero 
 * @author Anthony Eduardo Alvarado Carbajal
 * @param pipe_fd Tubería que comunica al registrador
 * @param target_ini: El objetivo
 * @param rounds Nº de rondas
 * @param n_threads Nº de hilos en cada ronda
 * @param pid_hijo El PID del (registrador)
 * 
 */
void ejecutar_minero(int pipe_fd[2], long int target_ini, int rounds, int n_threads, pid_t pid_hijo) {
    close(pipe_fd[0]);  
    
    printf("[Minero] Proceso iniciado (PID: %d)\n", getpid());
    printf("[Minero] Resolviendo %d rondas con %d hilos\n", rounds, n_threads);
    printf("[Minero] Límite de búsqueda: POW_LIMIT = %d\n", POW_LIMIT);
    
    long int current_target = target_ini;
    

    for (int ronda = 0; ronda < rounds; ronda++) {
        printf("[Minero] Ronda %d: Buscando solución para target = %ld\n", 
               ronda + 1, current_target);
        
        // Variables compartidas entre los hilos de esta ronda
        long int solution = 0;      
        int found = 0;              
        pthread_mutex_t mutex;      
        
        if (pthread_mutex_init(&mutex, NULL) != 0) {
            perror("pthread_mutex_init");
            exit(EXIT_FAILURE);
        }
        
        long int range_per_thread = POW_LIMIT / n_threads;  
        long int remainder = POW_LIMIT % n_threads;         
        
        pthread_t threads[n_threads];
        thread_args_t args[n_threads];
        long int start = 0;  
        // Crea los hilos
        for (int i = 0; i < n_threads; i++) {
            args[i].target = current_target;
            args[i].start = start;
            
            long int extra = (i < remainder) ? 1 : 0;
            args[i].end = start + range_per_thread + extra;
            
            args[i].solution = &solution;
            args[i].found = &found;
            args[i].mutex = &mutex;
            
            if (pthread_create(&threads[i], NULL, miner_thread, &args[i]) != 0) {
                perror("pthread_create");
                exit(EXIT_FAILURE);
            }
            
            start = args[i].end;
        }

        for (int i = 0; i < n_threads; i++) {
            pthread_join(threads[i], NULL);
        }
        
        pthread_mutex_destroy(&mutex);
        
        // Comprueba si se encuentra una solución
        if (found) {
            printf("Solution accepted: %08ld --> %08ld\n", current_target, solution);
            current_target = solution;  
        } else {
            fprintf(stderr, "[Minero] ERROR: No se encontró solución para target %ld\n", 
                    current_target);
            exit(EXIT_FAILURE);
        }
    }
    
    char msg[] = "Minado completado con éxito!";
    write(pipe_fd[1], msg, strlen(msg) + 1);
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
    // Control de argumentos
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
    if (target_ini < 0 || target_ini >= POW_LIMIT) {
        fprintf(stderr, "ERROR: Target debe estar entre 0 y %d\n", POW_LIMIT - 1);
        exit(EXIT_FAILURE);
    }
    
    // Crea una tuberia
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    
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
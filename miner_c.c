/**
 * @brief Apartado c
 * @author Anthony Eduardo Alvarado Carbajal
 * @date 25-02-2026
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include "pow.h"

#define MAX_BUFFER 256
#define LOG_FILENAME_SIZE 64
#define VALIDATION_PROBABILITY 20

/**
 * @brief: Estructura para pasarle los datos a cada hilo
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
 * @brief Minero hilo
 * @author Anthony Eduardo Alvarado Carbajal
 * @param arg Puntero tipo hilo
 */
void *miner_thread(void *arg) {
    thread_args_t *args = (thread_args_t *)arg;
    long int i;
    int solution_found;
    
    for (i = args->start; i < args->end; i++) {
        pthread_mutex_lock(args->mutex);
        solution_found = *(args->found);
        pthread_mutex_unlock(args->mutex);
        
        if (solution_found) {
            pthread_exit(NULL);
        }
        
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
 * @brief Obtiene el estado de validación
 * @author Anthony Eduardo Alvarado Carbajal
 */
const char *get_validation_status(void) {
    int random;
    random = rand() % 100;
    if (random < VALIDATION_PROBABILITY) {
        return "(rejected)";
    } else {
        return "(validated)";
    }
}

/**
 * @brief Escribe el mensaje esperado segun el enunciado
 * @author Anthony Eduardo Alvarado Carbajal
 * @param log_file archivo que se usa
 * @param ronda Nº de la ronda actual
 * @param target Objetivo
 * @param solucion Solución a buscar
 * @param ppid PID del proceso padre (minero)
 * @param status Estado de validación: "(validated)" o "(rejected)"
 */
void escribir_bloque(FILE *log_file, int ronda, long int target, 
                     long int solucion, pid_t ppid, const char *status) {
    fprintf(log_file, "Id: %d\n", ronda);
    fprintf(log_file, "Winner: %d\n", ppid);
    fprintf(log_file, "Target: %ld\n", target);
    fprintf(log_file, "Solution: %ld %s\n", solucion, status);
    fprintf(log_file, "Votes: %d/%d\n", ronda, ronda);
    fprintf(log_file, "Wallets: %d:%d\n", ppid, ronda);
    fprintf(log_file, "\n");
}


/**
 * @brief Ejecuta el registrador
 * @author Anthony Eduardo Alvarado Carbajal
 * @param pipe_fd extremos de la tubería
 */
void ejecutar_registrador(int pipe_fd[2]) {
    pid_t ppid;
    char log_filename[LOG_FILENAME_SIZE];
    FILE *log_file;
    char buffer[MAX_BUFFER];
    ssize_t bytes;
    int ronda;
    long int target, solucion;
    int ronda_recibida;
    const char *status;
    
    close(pipe_fd[1]);
    
    ppid = getppid();
    

    snprintf(log_filename, sizeof(log_filename), "%d.log", ppid);
    

    log_file = fopen(log_filename, "w");
    if (!log_file) {
        perror("[Registrador] Error abriendo fichero");
        exit(EXIT_FAILURE);
    }
    
    printf("[Registrador] PID=%d, PPID=%d, Fichero=%s\n", getpid(), ppid, log_filename);
    
    srand(time(NULL) ^ (getpid() << 16));
    
    ronda = 1;
    
    /* Leer mensajes del minero */
    while ((bytes = read(pipe_fd[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes] = '\0';
        buffer[strcspn(buffer, "\n")] = '\0';
        
        if (sscanf(buffer, "%d:%ld:%ld", &ronda_recibida, &target, &solucion) == 3) {
            if (ronda_recibida != ronda) {
                fprintf(stderr, "[Registrador] Error: Ronda %d esperada, llegó %d\n", 
                        ronda, ronda_recibida);
                break;
            }
            
            status = get_validation_status();
            
            /* ESCRIBIR EN EL FICHERO (NO en terminal) */
            escribir_bloque(log_file, ronda, target, solucion, ppid, status);
            fflush(log_file);
            
            /* Solo mostramos por terminal información básica */
            printf("[Registrador] Ronda %d registrada\n", ronda);
            
            ronda++;
        }
    }
    
    fclose(log_file);
    close(pipe_fd[0]);
    printf("[Registrador] Terminando\n");
    exit(EXIT_SUCCESS);
}


/**
 * @brief Resuelve una ronda del minero
 * @author Anthony Eduardo Alvarado Carbajañ
 * @param target el objetivo
 * @param n_threads el nº de hilos 
 * @return Solucion
 */
long int resolver_ronda(long int target, int n_threads) {
    long int solution;
    int found;
    pthread_mutex_t mutex;
    long int range_per_thread;
    long int remainder;
    pthread_t threads[n_threads];
    thread_args_t args[n_threads];
    long int start;
    int i;
    long int extra;
    
    solution = 0;
    found = 0;
    pthread_mutex_init(&mutex, NULL);
    
    range_per_thread = POW_LIMIT / n_threads;
    remainder = POW_LIMIT % n_threads;
    
    start = 0;
    
    for (i = 0; i < n_threads; i++) {
        args[i].target = target;
        args[i].start = start;
        
        extra = (i < remainder) ? 1 : 0;
        args[i].end = start + range_per_thread + extra;
        
        args[i].solution = &solution;
        args[i].found = &found;
        args[i].mutex = &mutex;
        
        pthread_create(&threads[i], NULL, miner_thread, &args[i]);
        start = args[i].end;
    }
    
    for (i = 0; i < n_threads; i++) {
        pthread_join(threads[i], NULL);
    }
    
    pthread_mutex_destroy(&mutex);
    
    if (!found) {
        fprintf(stderr, "[Minero] ERROR: No se encontró solución\n");
        exit(EXIT_FAILURE);
    }
    
    return solution;
}

/** 
 * @brief Ejecuta el proceso minero
 * @author Anthony Eduardo Alvarado Carbajal
 * @param pipe_fd extremos de la tuberia
 * @param target_ini el obejtivo
 * @param rounds Nº de rondas
 * @param n_threads Nº de hilos por ronda
 * @param pid_hijo PID del proceso hijo(regitrador)
 * 
 */

void ejecutar_minero(int pipe_fd[2], long int target_ini, int rounds, int n_threads, pid_t pid_hijo) {
    long int current_target;
    char buffer[MAX_BUFFER];
    int ronda;
    long int solution;
    ssize_t bytes;
    int status;
    
    close(pipe_fd[0]);
    
    printf("[Minero] PID=%d, Rondas=%d, Hilos=%d\n", getpid(), rounds, n_threads);
    
    current_target = target_ini;
    
    for (ronda = 1; ronda <= rounds; ronda++) {
        printf("[Minero] Ronda %d: target=%ld\n", ronda, current_target);
        
        solution = resolver_ronda(current_target, n_threads);
        
        printf("Solution accepted: %ld --> %ld\n", current_target, solution);
        
        snprintf(buffer, sizeof(buffer), "%d:%ld:%ld", ronda, current_target, solution);
        bytes = write(pipe_fd[1], buffer, strlen(buffer) + 1);
        if (bytes == -1) {
            perror("[Minero] Error enviando");
            exit(EXIT_FAILURE);
        }
        
        current_target = solution;
    }
    
    close(pipe_fd[1]);
    
    waitpid(pid_hijo, &status, 0);
    
    if (WIFEXITED(status)) {
        printf("Logger exited with status %d\n", WEXITSTATUS(status));
    } else {
        printf("Logger exited unexpectedly\n");
    }
    
    printf("[Minero] Terminando\n");
    exit(EXIT_SUCCESS);
}

/**
 * @brief Programa Principal
 * @author Anthony Eduardo Alvarado
 */
int main(int argc, char *argv[]) {
    long int target_ini;
    int rounds;
    int n_threads;
    int pipe_fd[2];
    pid_t pid;
    
    if (argc != 4) {
        fprintf(stderr, "Uso: %s <TARGET_INI> <ROUNDS> <N_THREADS>\n", argv[0]);
        fprintf(stderr, "Ejemplo: %s 0 5 4\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    target_ini = atol(argv[1]);
    rounds = atoi(argv[2]);
    n_threads = atoi(argv[3]);
    
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return EXIT_FAILURE;
    }
    
    pid = fork();
    
    if (pid < 0) {
        perror("fork");
        return EXIT_FAILURE;
    } else if (pid == 0) {
        ejecutar_registrador(pipe_fd);
    } else {
        ejecutar_minero(pipe_fd, target_ini, rounds, n_threads, pid);
    }
    
    return 0;
}
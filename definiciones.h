#ifndef DEFINICIONES_H
#define DEFINICIONES_H

#include <pthread.h>
#include <semaphore.h>
#include <sys/types.h>
#include <signal.h>

#define SHM_SISTEMA  "/shm_soper"
#define MQ_NAME      "/mq_soper"

#define MAX_MINEROS   100
#define BUFFER_SIZE     6
#define MQ_MAX_MSG      7

typedef struct {
    long  target;
    long  solution;
    pid_t winner_pid;
    int   n_voters;
    char  votos[MAX_MINEROS];
    int   valida;
    int   finalizar;
} Bloque;

typedef struct {
    long target_actual;
    long solution_actual;

    pid_t procesos_activos[MAX_MINEROS];
    int   n_activos;

    pid_t pids_carteras[MAX_MINEROS];
    int   monedas[MAX_MINEROS];
    int   n_carteras;

    char  votos[MAX_MINEROS];
    int   n_votos;

    sem_t sem_sistema;

    Bloque b[BUFFER_SIZE];
    int    in;
    int    out;

    sem_t  s_empty;
    sem_t  s_fill;
    sem_t  s_mtx;
} ShmSistema;

typedef struct {
    int target;
    int solution;
    int ronda;
    int votos_totales;
    int votos_aceptados;
    int aceptado;
    int monedas_ganador;
} Message;

typedef struct {
    int  start;
    int  stop;
    int  target;
    int* found;
    pthread_mutex_t* mutex_found;
} Rank;

typedef struct {
    int  yes;
    int  no;
    int  aceptado;
    char lista[512];
} VotoResultado;

#endif
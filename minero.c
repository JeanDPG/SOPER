/* ============================================================================
 *  minero.c
 *  Lógica principal del proceso de minado
 * ============================================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <mqueue.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "minero.h"
#include "definiciones.h"
#include "pow.h"

/* Wrapper para evitar que las señales rompan la sincronización */
static void safe_sem_wait(sem_t *sem) {
    while ((sem_wait)(sem) == -1) {
        if (errno != EINTR) {
            perror("sem_wait");
            exit(EXIT_FAILURE);
        }
    }
}
#define sem_wait safe_sem_wait

#define MAX_VOTOS_INTENTOS 5

/* ----------------- flags de señales ----------------- */
volatile sig_atomic_t got_SIGALRM = 0;
volatile sig_atomic_t got_SIGUSR1 = 0;
volatile sig_atomic_t got_SIGUSR2 = 0;

void handler(int sig) {
    if      (sig == SIGALRM) got_SIGALRM = 1;
    else if (sig == SIGUSR1) got_SIGUSR1 = 1;
    else if (sig == SIGUSR2) got_SIGUSR2 = 1;
}

/* ===========================================================================
 *  HELPERS sobre la memoria compartida
 *  Todos asumen que el llamante ya tiene cogido shm->sem_sistema
 *  (salvo donde se indica lo contrario).
 * =========================================================================== */

/* Añade un nuevo minero al sistema */
static int sistema_anadir_proceso(ShmSistema *shm, pid_t pid) {
    if (shm->n_activos >= MAX_MINEROS) return -1;
    shm->procesos_activos[shm->n_activos++] = pid;
    return shm->n_activos;
}

/* Quita el PID de procesos_activos[]. Devuelve el nuevo n_activos. */
static int sistema_quitar_proceso(ShmSistema *shm, pid_t pid) {
    int i;
    for (i = 0; i < shm->n_activos; i++) {
        if (shm->procesos_activos[i] == pid) {
            /* Compacta moviendo los siguientes una posición */
            for (int j = i; j < shm->n_activos - 1; j++)
                shm->procesos_activos[j] = shm->procesos_activos[j+1];
            shm->n_activos--;
            return shm->n_activos;
        }
    }
    return shm->n_activos;
}

/* Imprime "  Miners: [ pid1 pid2 ... ]" leyendo procesos_activos[].
   El llamante debe tener cogido sem_sistema. */
static void sistema_listar_mineros(ShmSistema *shm) {
    printf("  Miners: [ ");
    for (int i = 0; i < shm->n_activos; i++) {
        printf("%d ", shm->procesos_activos[i]);
    }
    printf("]\n");
    fflush(stdout);
}

/* Incrementa la cartera del proceso pid, creando entrada si no existe.
   El llamante debe tener cogido sem_sistema. Devuelve las monedas. */
static int sistema_incrementar_cartera(ShmSistema *shm, pid_t pid) {
    for (int i = 0; i < shm->n_carteras; i++) {
        if (shm->pids_carteras[i] == pid) {
            shm->monedas[i]++;
            return shm->monedas[i];
        }
    }
    /* Nueva entrada */
    if (shm->n_carteras < MAX_MINEROS) {
        shm->pids_carteras[shm->n_carteras] = pid;
        shm->monedas[shm->n_carteras] = 1;
        shm->n_carteras++;
        return 1;
    }
    return 0;
}

/* Notifica con kill(pid, sig) a todos los mineros activos salvo a mí.
   El llamante debe tener cogido sem_sistema. */
static void sistema_notificar_a_todos(ShmSistema *shm, pid_t mi_pid, int sig) {
    for (int i = 0; i < shm->n_activos; i++) {
        if (shm->procesos_activos[i] != mi_pid)
            kill(shm->procesos_activos[i], sig);
    }
}

/* Cuenta votos en shm->votos[] (sem_sistema cogido por el llamante). */
static int sistema_contar_votos(ShmSistema *shm) {
    return shm->n_votos;
}

/* Limpia los votos para una nueva ronda (sem_sistema cogido). */
static void sistema_limpiar_votos(ShmSistema *shm) {
    shm->n_votos = 0;
    memset(shm->votos, 0, sizeof(shm->votos));
}

/* ===========================================================================
 *  Búsqueda paralela del objetivo
 * =========================================================================== */
void* powRank(void* arg) {
    Rank* r = (Rank*) arg;
    for (int i = r->start; i < r->stop; i++) {
        pthread_mutex_lock(r->mutex_found);
        int actual = *(r->found);
        pthread_mutex_unlock(r->mutex_found);
        if (actual != -1) {
            free(r);
            return NULL;
        }
        if (pow_hash(i) == r->target) {
            pthread_mutex_lock(r->mutex_found);
            if (*(r->found) == -1) {
                *(r->found) = i;
            }
            pthread_mutex_unlock(r->mutex_found);
            free(r);
            return NULL;
        }
    }
    free(r);
    return NULL;
}

/* ===========================================================================
 *  Función VOTAR: añade un voto (Y/N) en shm->votos[] de forma protegida.
 *  Esta función NO debe ser llamada por el ganador.
 * =========================================================================== */
static void votar(ShmSistema *shm, int target_a_validar) {
    sem_wait(&shm->sem_sistema);
    long sol = shm->solution_actual;
    char voto = (pow_hash((int)sol) == target_a_validar) ? 'Y' : 'N';
    if (shm->n_votos < MAX_MINEROS) {
        shm->votos[shm->n_votos++] = voto;
    }
    sem_post(&shm->sem_sistema);
}

/* ===========================================================================
 *  Recolección final de votos: imprime Winner... y devuelve VotoResultado.
 *  Llamada SIN tener sem_sistema cogido.
 * =========================================================================== */
static VotoResultado contar_y_mostrar_votos(ShmSistema *shm,
                                            pid_t ganador_pid,
                                            int *monedas) {
    sem_wait(&shm->sem_sistema);
    int n   = shm->n_votos;
    int yes = 0, no = 0;
    char lista[512] = "";
    for (int i = 0; i < n; i++) {
        if (shm->votos[i] == 'Y') { yes++; strcat(lista, "Y "); }
        else if (shm->votos[i] == 'N') { no++; strcat(lista, "N "); }
    }
    int aceptado = (yes >= no);
    if (aceptado) {
        *monedas = sistema_incrementar_cartera(shm, ganador_pid);
    }
    sem_post(&shm->sem_sistema);

    printf("Winner %d => [ %s] => %s\n", ganador_pid, lista,
           aceptado ? "Accepted" : "Rejected");
    fflush(stdout);

    VotoResultado res;
    res.yes = yes;
    res.no = no;
    res.aceptado = aceptado;
    strcpy(res.lista, lista);
    return res;
}

/* ===========================================================================
 *  EJECUTAR_MINERO  --  Punto de entrada del proceso minero
 * =========================================================================== */
void ejecutar_minero(int n_secs, int n_threads,
                     int pipe_escritura, int pipe_lectura,
                     ShmSistema *shm, mqd_t mq) {
    pid_t pid = getpid();
    sigset_t mask_all, mask_old;
    struct sigaction act;
    int ganador = 0, monedas = 0, ronda = 0;

    /* ---- Configurar máscaras de señales ANTES de instalar handler ---- */
    sigemptyset(&mask_all);
    sigaddset(&mask_all, SIGALRM);
    sigaddset(&mask_all, SIGUSR1);
    sigaddset(&mask_all, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask_all, &mask_old);

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    sigaddset(&act.sa_mask, SIGALRM);
    sigaddset(&act.sa_mask, SIGUSR1);
    sigaddset(&act.sa_mask, SIGUSR2);
    act.sa_flags = 0;
    sigaction(SIGALRM, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);

    /* ---- Inscribirse en el sistema (sección crítica) ---- */
    sem_wait(&shm->sem_sistema);
    if (shm->n_activos == 0) ganador = 1;     /* Soy el primer minero */
    sistema_anadir_proceso(shm, pid);
    printf("Miner %d added to system\n", pid);
    fflush(stdout);
    sistema_listar_mineros(shm);
    sem_post(&shm->sem_sistema);

    alarm(n_secs);

    /* ---- BUCLE PRINCIPAL DE RONDAS ---- */
    while (!got_SIGALRM) {

        /* ---------- INICIO DE RONDA ---------- */
        if (ganador) {
            /* Espera no activa hasta que haya >= 2 mineros */
            while (!got_SIGALRM) {
                sem_wait(&shm->sem_sistema);
                int n = shm->n_activos;
                sem_post(&shm->sem_sistema);
                if (n > 1) break;
                sigprocmask(SIG_SETMASK, &mask_old, NULL);
                sleep(1);
                sigprocmask(SIG_SETMASK, &mask_all, NULL);
            }
            if (got_SIGALRM) break;

            /* Limpia votos y notifica a todos para empezar la ronda */
            sem_wait(&shm->sem_sistema);
            sistema_limpiar_votos(shm);
            sistema_notificar_a_todos(shm, pid, SIGUSR1);
            sem_post(&shm->sem_sistema);
            got_SIGUSR1 = 1;
        } else {
            while (!got_SIGUSR1 && !got_SIGALRM)
                sigsuspend(&mask_old);
            if (got_SIGALRM) break;
        }

        ronda++;

        /* Lee el target actual de memoria compartida */
        sem_wait(&shm->sem_sistema);
        int target_ronda = (int) shm->target_actual;
        sem_post(&shm->sem_sistema);

        /* ---------- MINADO PARALELO ---------- */
        int found = -1;
        pthread_mutex_t mutex_found;
        pthread_mutex_init(&mutex_found, NULL);
        pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
        int range = POW_LIMIT / n_threads;
        for (int i = 0; i < n_threads; i++) {
            Rank* r = malloc(sizeof(Rank));
            r->start  = i * range;
            r->stop   = (i + 1) * range;
            r->target = target_ronda;
            r->found  = &found;
            r->mutex_found = &mutex_found;
            pthread_create(&threads[i], NULL, powRank, r); /* lanzamos el hilo */
        }
        for (int i = 0; i < n_threads; i++)
            pthread_join(threads[i], NULL);
        free(threads);
        pthread_mutex_destroy(&mutex_found);

        /* ---------- DETERMINACIÓN ATÓMICA DEL GANADOR ----------
           Solo el primer minero que adquiera el sem y vea
           target_actual == target_ronda gana. El resto, al releer,
           verán target distinto y pasarán a votantes.            */
        int soy_ganador = 0;
        if (found != -1) {
            sem_wait(&shm->sem_sistema);
            if (shm->target_actual == (long)target_ronda) {
                shm->target_actual  = (long) found;   /* nuevo target = solución */
                shm->solution_actual = (long) found;
                sistema_notificar_a_todos(shm, pid, SIGUSR2);
                soy_ganador = 1;
                got_SIGUSR2 = 1;
            }
            sem_post(&shm->sem_sistema);
        }

        if (soy_ganador) {
            /* --- Camino del GANADOR --- */
            /* El ganador NO vota, solo recolecta los votos de los demás. */

            /* Espera N-1 votos, hasta MAX_VOTOS_INTENTOS segundos */
            int intentos = 0;
            int votos_insuficientes = 1;
            while (votos_insuficientes && intentos < MAX_VOTOS_INTENTOS && !got_SIGALRM) {
                sem_wait(&shm->sem_sistema);
                int votos_actuales   = sistema_contar_votos(shm);
                int mineros_actuales = shm->n_activos;
                sem_post(&shm->sem_sistema);
                if (votos_actuales >= mineros_actuales - 1) {
                    votos_insuficientes = 0;
                } else {
                    sigprocmask(SIG_SETMASK, &mask_old, NULL);
                    sleep(1);
                    sigprocmask(SIG_SETMASK, &mask_all, NULL);
                    intentos++;
                }
            }

            VotoResultado votos = contar_y_mostrar_votos(shm, pid, &monedas);

            /* ---- Enviar bloque al Comprobador (cola de mensajes) ---- */
            Bloque b;
            memset(&b, 0, sizeof(Bloque));
            b.target     = (long) target_ronda;
            b.solution   = (long) found;
            b.winner_pid = pid;
            b.n_voters   = votos.yes + votos.no;
            for (int i = 0; i < b.n_voters && i < MAX_MINEROS; i++) {
                /* Reconstruimos los votos: yes primero, no después
                   (el orden exacto no importa para la validación)   */
                b.votos[i] = (i < votos.yes) ? 'Y' : 'N';
            }
            b.valida    = 0;   /* lo pondrá el Comprobador */
            b.finalizar = 0;
            mq_send(mq, (const char *)&b, sizeof(Bloque), 1); // enviamos el bloque

            /* ---- Comunicar resultado al registrador local por pipe ---- */
            Message msg = {
                target_ronda,
                found,
                ronda,
                votos.yes + votos.no,
                votos.yes,
                votos.aceptado,
                monedas
            };
            write(pipe_escritura, &msg, sizeof(Message));
            char ack;
            read(pipe_lectura, &ack, 1);

            ganador = 1;

        } else {
            /* --- Camino del VOTANTE --- */
            while (!got_SIGUSR2 && !got_SIGALRM)
                sigsuspend(&mask_old);
            if (got_SIGALRM) break;
            votar(shm, target_ronda);
            ganador = 0;
        }

        got_SIGUSR1 = 0;
        got_SIGUSR2 = 0;
    }

    /* ---------- FINAL: salir del sistema ----------
       Cada minero saldra por su propia SIGALRM.                                            */
    sem_wait(&shm->sem_sistema);
    sistema_quitar_proceso(shm, pid);
    int restantes = shm->n_activos;
    printf("Miner %d exited system\n", pid);
    fflush(stdout);
    if (restantes > 0) sistema_listar_mineros(shm);
    sem_post(&shm->sem_sistema);

    /* Si soy el último minero del sistema -> bloque de finalización */
    if (restantes == 0) {
        Bloque fin;
        memset(&fin, 0, sizeof(Bloque));
        fin.finalizar = 1;
        fin.winner_pid = pid;
        mq_send(mq, (const char *)&fin, sizeof(Bloque), 1); 
        /* También avisamos al registrador local con solution=-1 */
        Message m_fin = {0, -1, 0, 0, 0, 0, 0};
        write(pipe_escritura, &m_fin, sizeof(Message));
    } else {
        /* Avisamos solo a nuestro registrador local de que terminamos */
        Message m_fin = {0, -1, 0, 0, 0, 0, 0};
        write(pipe_escritura, &m_fin, sizeof(Message));
    }

    close(pipe_escritura);
    close(pipe_lectura);

    printf("Minero [PID: %d] finalizó. Monedas: %d\n", pid, monedas);
    fflush(stdout);
    exit(EXIT_SUCCESS);
}
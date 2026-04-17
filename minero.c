#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>
#include <signal.h>
#include <string.h>

#include "minero.h"
#include "definiciones.h"
#include "pow.h"

#define SEM_NAME    "/mutex_minero"
#define FILE_PIDS   "processPID.txt"
#define FILE_TARGET "target.txt"
#define FILE_VOTOS  "votos.txt"
#define MAX_VOTOS_INTENTOS 50


volatile sig_atomic_t got_SIGALRM = 0;
volatile sig_atomic_t got_SIGUSR1 = 0;
volatile sig_atomic_t got_SIGUSR2 = 0;

void handler(int sig) {
    if      (sig == SIGALRM) got_SIGALRM = 1;
    else if (sig == SIGUSR1) got_SIGUSR1 = 1;
    else if (sig == SIGUSR2) got_SIGUSR2 = 1;
}

/* --- 1. FUNCIÓN: powRank (Hilos) --- */
void* powRank(void* arg) {
    Rank* r = (Rank*) arg;
    for (int i = r->start; i < r->stop; i++) {
        if (*(r->found) != -1) {
            free(r);
            return NULL;
        }
        if (pow_hash(i) == r->target) {
            if (*(r->found) == -1) {
                *(r->found) = i;
            }
            free(r);
            return NULL;
        }
    }
    free(r);
    return NULL;
}

/* --- 2. FUNCIÓN: escritura_de_ficheroPIDS --- */
void escritura_de_ficheroPIDS(pid_t pid) {
    int fd = open(FILE_PIDS, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd == -1) {
        perror("open processPID.txt");
        exit(EXIT_FAILURE);
    }
    char buffer[64];
    int len = snprintf(buffer, sizeof(buffer), "%d\n", pid);
    write(fd, buffer, len);
    close(fd);
}

/* --- 3. FUNCIÓN: leer_Target --- */
void leer_Target(pid_t pid, int *init_target) {
    int fd3 = open(FILE_TARGET, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd3 != -1) {
        char buffer[64];
        int len = snprintf(buffer, sizeof(buffer), "%d\n", *init_target);
        write(fd3, buffer, len);
        close(fd3);
    } else if (errno == EEXIST) {
        fd3 = open(FILE_TARGET, O_RDONLY);
        if (fd3 != -1) {
            char read_buffer[64];
            int n = read(fd3, read_buffer, sizeof(read_buffer)-1);
            if (n > 0) {
                read_buffer[n] = '\0';
                *init_target = atoi(read_buffer);
            }
            close(fd3);
        }
    }
}

/* --- 4. FUNCIÓN: escribir_Target ---
   Escritura atómica mediante fichero temporal + rename(). */
void escribir_Target(int nuevo_target) {
    const char *tmp = "target.tmp";
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd != -1) {
        char buffer[32];
        int len = snprintf(buffer, sizeof(buffer), "%d\n", nuevo_target);
        write(fd, buffer, len);
        close(fd);
        rename(tmp, FILE_TARGET);
    }
}

/* --- 5. FUNCIÓN: contar_votos --- */
int contar_votos() {
    int fd = open(FILE_VOTOS, O_RDONLY);
    if (fd == -1) return 0;
    int count = 0; char c;
    while (read(fd, &c, 1) > 0) {
        if (c == 'Y' || c == 'N') count++;
    }
    close(fd);
    return count;
}

/* --- 6. FUNCIÓN: votar --- */
void votar(int target_a_validar, sem_t *sem) {
    sem_wait(sem);

    int fd_read = open(FILE_TARGET, O_RDONLY);
    char buf[32] = {0};
    if (fd_read != -1) {
        int n = read(fd_read, buf, sizeof(buf)-1);
        if (n > 0) buf[n] = '\0';
        close(fd_read);
    }
    int sol = atoi(buf);

    char voto = (pow_hash(sol) == target_a_validar) ? 'Y' : 'N';

    int fd_write = open(FILE_VOTOS, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd_write != -1) {
        write(fd_write, &voto, 1);
        close(fd_write);
    }

    sem_post(sem);
}

/* --- 7. FUNCIÓN: contar_y_mostrar_votos --- */
int contar_y_mostrar_votos(pid_t ganador_pid, int *monedas, sem_t *sem) {
    sem_wait(sem);
    int fd = open(FILE_VOTOS, O_RDONLY);
    if (fd == -1) { sem_post(sem); return 0; }
    char v_buf[256];
    int n = read(fd, v_buf, sizeof(v_buf)-1);
    close(fd);
    sem_post(sem);
    v_buf[n] = '\0';

    int yes = 0, no = 0;
    char lista[512] = "";
    for (int i = 0; i < n; i++) {
        if (v_buf[i] == 'Y') { yes++; strcat(lista, "Y "); }
        else if (v_buf[i] == 'N') { no++; strcat(lista, "N "); }
    }
    int aceptado = (yes >= no);
    printf("Winner %d => [ %s] => %s\n", ganador_pid, lista, aceptado ? "Accepted" : "Rejected");
    fflush(stdout);
    if (aceptado) (*monedas)++;
    return aceptado;
}

/* --- 8. FUNCIÓN: contar_mineros_en_fichero --- */
int contar_mineros_en_fichero() {
    int fd = open(FILE_PIDS, O_RDONLY);
    if (fd == -1) return 0;
    int count = 0; char c, last_char = 0;
    while (read(fd, &c, 1) > 0) {
        if (c == '\n') count++;
        last_char = c;
    }
    // Si el último carácter no es '\n', hay un PID sin procesar
    if (last_char != '\n' && last_char != 0) count++;
    close(fd);
    return count;
}

/* --- 9. FUNCIÓN: notificar_a_todos --- */
void notificar_a_todos(pid_t mi_pid, int señal) {
    int fd = open(FILE_PIDS, O_RDONLY);
    if (fd == -1) return;
    char c, buf_pid[16]; int i = 0;
    while (read(fd, &c, 1) > 0) {
        if (c == '\n') {
            buf_pid[i] = '\0';
            pid_t p = (pid_t)atoi(buf_pid);
            if (p > 0 && p != mi_pid) kill(p, señal);
            i = 0;
        } else if (i < 15) buf_pid[i++] = c;
    }
    // Procesar último PID si no termina en '\n'
    if (i > 0) {
        buf_pid[i] = '\0';
        pid_t p = (pid_t)atoi(buf_pid);
        if (p > 0 && p != mi_pid) kill(p, señal);
    }
    close(fd);
}

/* --- 9.5. FUNCIÓN: limpiar_votos --- */
void limpiar_votos() {
    unlink(FILE_VOTOS);
}

/* --- 10. FUNCIÓN: borrado_de_ficheros --- */
void borrado_de_ficheros(pid_t pid_to_delete) {
    char aux[64];
    snprintf(aux, sizeof(aux), "processPID_aux_%d.txt", (int)pid_to_delete);

    int fd1 = open(FILE_PIDS, O_RDONLY);
    int fd2 = open(aux, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd1 == -1) { if (fd2 != -1) close(fd2); return; }
    char c, line[64]; int j = 0, restantes = 0;
    while (read(fd1, &c, 1) > 0) {
        if (c == '\n') {
            line[j] = '\0';
            if ((pid_t)atoi(line) != pid_to_delete) {
                restantes++;
                dprintf(fd2, "%s\n", line);
            }
            j = 0;
        } else if (j < 63) line[j++] = c;
    }
    // Procesar último PID si no termina en '\n'
    if (j > 0) {
        line[j] = '\0';
        if ((pid_t)atoi(line) != pid_to_delete) {
            restantes++;
            dprintf(fd2, "%s\n", line);
        }
    }
    close(fd1); close(fd2);
    unlink(FILE_PIDS);
    if (restantes == 0) {
        unlink(aux);
        unlink(FILE_TARGET);
        unlink(FILE_VOTOS);
    } else {
        rename(aux, FILE_PIDS);
    }
}

void ejecutar_minero(int n_secs, int n_threads, int pipe_escritura, int pipe_lectura) {
    pid_t pid = getpid();
    sem_t *sem;
    sigset_t mask_all, mask_old;
    struct sigaction act;
    int ganador = 0, init_target = 0, monedas = 0, ronda = 0, n;

    act.sa_handler = handler;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGALRM, &act, NULL);
    sigaction(SIGUSR1, &act, NULL);
    sigaction(SIGUSR2, &act, NULL);

    sigemptyset(&mask_all);
    //sigaddset(&mask_all, SIGALRM);
    sigaddset(&mask_all, SIGUSR1);
    sigaddset(&mask_all, SIGUSR2);
    sigprocmask(SIG_BLOCK, &mask_all, &mask_old);

    sem = sem_open(SEM_NAME, O_CREAT, 0644, 1);
    if (sem == SEM_FAILED) {
        perror("sem_open");
        exit(EXIT_FAILURE);
    }

    sem_wait(sem);
    if (contar_mineros_en_fichero() == 0) ganador = 1;
    escritura_de_ficheroPIDS(pid);
    leer_Target(pid, &init_target);
    sem_post(sem);

    alarm(n_secs);

    while (!got_SIGALRM) {
        /* ------------------ INICIO DE RONDA ------------------ */
        if (ganador) {
            /*sem_wait(sem);
             n = contar_mineros_en_fichero();
            sem_post(sem);*/
            while (!got_SIGALRM) {
               int n = contar_mineros_en_fichero();
                if (n <= 1 && ronda > 0) {
                    sleep(1);
                }
                else break;
            }
           /* if (got_SIGALRM) break;
            sem_wait(sem);
             n = contar_mineros_en_fichero();
            sem_post(sem);
            if(n < 2) break;
            */
            sem_wait(sem);
            limpiar_votos();
            notificar_a_todos(pid, SIGUSR1);
            sem_post(sem);

            got_SIGUSR1 = 1;
        } else {
            while (!got_SIGUSR1 && !got_SIGALRM)
                sigsuspend(&mask_old);
        }

        /*if (got_SIGALRM) break;
        sem_wait(sem);
        n = contar_mineros_en_fichero();
        sem_post(sem);
        if(n < 2) break;
        */
        got_SIGUSR1 = 0;
        ronda++;

        sem_wait(sem);
        leer_Target(pid, &init_target);
        sem_post(sem);

        /* ------------------ MINADO ------------------ */

        int target_ronda = init_target;
        int found = -1;

        pthread_t *threads = malloc(n_threads * sizeof(pthread_t));
        int range = POW_LIMIT / n_threads;

        for (int i = 0; i < n_threads; i++) {
            Rank* r = malloc(sizeof(Rank));
            r->start  = i * range;
            r->stop   = (i + 1) * range;
            r->target = target_ronda;
            r->found  = &found;
            if((pthread_create(&threads[i], NULL, powRank, r) != 0)){
                free(threads);
                exit(EXIT_FAILURE);
            }
        }

        /*while (found == -1 && !got_SIGUSR2 && !got_SIGALRM)
            usleep(100000);
        */
        for (int i = 0; i < n_threads; i++)
            pthread_join(threads[i], NULL);

        free(threads);

        //if (got_SIGALRM) break;

        /* ------------------ RESULTADO DE RONDA ------------------ */
        if (found != -1 && !got_SIGUSR2) {

            /* PASO 1: escribir target atómicamente (dentro del semáforo) */
            sem_wait(sem);
            escribir_Target(found);
            sem_post(sem);


            /* PASO 2: comunicar al monitor por pipe (FUERA del semáforo) */
            Message msg = {target_ronda, found, ronda};
            write(pipe_escritura, &msg, sizeof(Message));
            char ack;
            read(pipe_lectura, &ack, 1);

            /* PASO 3: notificar votación (dentro del semáforo) */
            sem_wait(sem);
            notificar_a_todos(pid, SIGUSR2);
            sem_post(sem);

            got_SIGUSR2 = 0;
            votar(target_ronda, sem);

            /* PASO 4: esperar a que voten todos los mineros activos */
            int intentos = 0;
            while (intentos < MAX_VOTOS_INTENTOS && !got_SIGALRM) {
                sem_wait(sem);
                int votos_actuales   = contar_votos();
                int mineros_actuales = contar_mineros_en_fichero();
                sem_post(sem);
                if (votos_actuales >= mineros_actuales) break;
                usleep(100000);
                intentos++;
            }

            contar_y_mostrar_votos(pid, &monedas, sem);
            ganador = 1;

        } else {
            while (!got_SIGUSR2 && !got_SIGALRM)
                sigsuspend(&mask_old);
            //if (got_SIGALRM) break;

            votar(target_ronda, sem);
            got_SIGUSR2 = 0;
            ganador = 0;
        }
        
        
    }

    /* ------------------ FINAL ------------------
    
     */
    sem_wait(sem);
    notificar_a_todos(pid, SIGUSR1);  
    notificar_a_todos(pid, SIGUSR2);  
    borrado_de_ficheros(pid);
    int rest = contar_mineros_en_fichero();
    sem_post(sem);
    if (rest == 0){
        Message fin = {0, -1, 0};
        write(pipe_escritura, &fin, sizeof(Message));

        close(pipe_escritura);
        close(pipe_lectura);

        sem_close(sem);
        sem_unlink(SEM_NAME);
    }
    
    exit(EXIT_SUCCESS);
}
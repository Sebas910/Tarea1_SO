/* main.c */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

#include "tipos.h"
#include "dag.h"

volatile sig_atomic_t seremi_llegada = 0;

static void handle_sigint(int sig) {
    (void)sig;
    seremi_llegada = 1;
}

// Handler vacio: SIGCHLD se ignora por defecto y una senal ignorada no despierta a sigsuspend().
static void handle_sigchld(int sig) {
    (void)sig;
}

static void dormir_ms(int ms) {
    struct timespec ts, rem;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (ms % 1000) * 1000000L;
    while (nanosleep(&ts, &rem) == -1 && errno == EINTR) ts = rem;
}

// Lee exactamente sizeof(Message) bytes (un read puede devolver menos). 0 = ok, -1 = error/EOF.
static int leer_mensaje(int fd, Message *msg) {
    size_t got = 0;
    while (got < sizeof(*msg)) {
        ssize_t n = read(fd, (char*)msg + got, sizeof(*msg) - got);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        if (n == 0) return -1;
        got += (size_t)n;
    }
    return 0;
}

static int escribir_mensaje(int fd, const Message *msg) {
    size_t sent = 0;
    while (sent < sizeof(*msg)) {
        ssize_t n = write(fd, (const char*)msg + sent, sizeof(*msg) - sent);
        if (n < 0) { if (errno == EINTR) continue; return -1; }
        sent += (size_t)n;
    }
    return 0;
}

// Codigo del proceso hijo: nunca retorna.
static void ejecutar_hijo(Task tasks[], int i, int wfd, const sigset_t *mascara_original) {
    // El hijo hereda la mascara y los handlers del padre: se restauran para que Ctrl+C lo mate normalmente.
    signal(SIGINT, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);
    sigprocmask(SIG_SETMASK, mascara_original, NULL);

    // Insumos recibidos de las actividades de las que depende
    for (int k = 0; k < tasks[i].num_src; k++) {
        Task *origen = &tasks[tasks[i].src[k]];
        printf("   [%s] recibio insumo de '%s': %s\n", tasks[i].name, origen->name, origen->out_msg);
    }
    fflush(stdout);

    dormir_ms(tasks[i].time_ms);

    // Falla interna simulada (opcional): PROB_FALLA=<0..100>. Por defecto no falla nunca.
    const char *pf = getenv("PROB_FALLA");
    if (pf) {
        srand((unsigned)getpid()); // cada hijo con semilla distinta
        if (rand() % 100 < atoi(pf)) _exit(1);
    }

    Message msg;
    memset(&msg, 0, sizeof(msg));
    snprintf(msg.id, sizeof(msg.id), "%s", tasks[i].id);
    snprintf(msg.text, sizeof(msg.text), "Insumo '%s' procesado y listo.", tasks[i].name);
    int rc = escribir_mensaje(wfd, &msg);
    close(wfd);
    _exit(rc == 0 ? 0 : 1); // _exit: no vacia los buffers de stdio heredados del padre (evitaba lineas duplicadas)
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s plan.txt K\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *filename = argv[1];
    int K = atoi(argv[2]);
    if (K <= 0) K = 1;

    srand(time(NULL));

    // Senales bloqueadas durante todo el bucle; solo se desbloquean de forma atomica dentro de sigsuspend().
    // Asi no existe la carrera "llega SIGINT justo antes de bloquearse en wait()".
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sigemptyset(&sa.sa_mask);
    sa.sa_handler = handle_sigint;
    sigaction(SIGINT, &sa, NULL);
    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    sigset_t bloqueadas, original;
    sigemptyset(&bloqueadas);
    sigaddset(&bloqueadas, SIGINT);
    sigaddset(&bloqueadas, SIGCHLD);
    sigprocmask(SIG_BLOCK, &bloqueadas, &original);

    Task* tasks = calloc(MAX_TASKS, sizeof(Task));
    if (!tasks) { perror("calloc"); return EXIT_FAILURE; }
    int total_tasks = 0;

    if (parse_plan(filename, tasks, &total_tasks) < 0) {
        free(tasks);
        return EXIT_FAILURE;
    }

    int active_procs = 0;
    int completed_nodes = 0;
    int aborted_nodes = 0;

    printf("Iniciando Planificador Dieciochero (Concurrencia: %d)...\n", K);
    fflush(stdout);

    while (completed_nodes + aborted_nodes < total_tasks) {
        // SIGINT esta bloqueada: se consulta si quedo pendiente para reaccionar de inmediato
        sigset_t pendientes;
        sigpending(&pendientes);
        if (sigismember(&pendientes, SIGINT)) seremi_llegada = 1;
        if (seremi_llegada) break;

        int tasks_started_this_cycle = 0;
        int fork_error = 0;

        // Control de Concurrencia
        for (int i = 0; i < total_tasks && active_procs < K; i++) {
            if (tasks[i].status == WAITING && tasks[i].deps_met == tasks[i].num_deps) {
                int fds[2];
                if (pipe(fds) == -1) { perror("pipe"); fork_error = 1; break; }

                fflush(stdout); // buffer vacio antes del fork: el hijo no hereda texto pendiente
                pid_t pid = fork();
                if (pid < 0) {
                    perror("fork");
                    close(fds[0]); close(fds[1]);
                    fork_error = 1;
                    break;
                }
                if (pid == 0) {
                    close(fds[0]);
                    ejecutar_hijo(tasks, i, fds[1], &original);
                }
                // PADRE
                close(fds[1]);
                tasks[i].rfd = fds[0];
                tasks[i].pid = pid;
                tasks[i].status = RUNNING;
                active_procs++;
                tasks_started_this_cycle++;
                printf("Iniciando: %s (PID: %d, Tiempo: %dms)\n", tasks[i].name, pid, tasks[i].time_ms);
            }
        }

        // Nada corriendo y nada que pueda arrancar: deadlock (o fork imposible)
        if (active_procs == 0 && tasks_started_this_cycle == 0) {
            if (fork_error)
                printf("\n Error Fatal: no se pudo crear procesos (fork/pipe).\n");
            else
                printf("\n Error Fatal: Bloqueo detectado (Deadlock). Hay dependencias imposibles de cumplir en plan.txt.\n");
            for (int i = 0; i < total_tasks; i++) {
                if (tasks[i].status == WAITING) {
                    tasks[i].status = ABORTED;
                    aborted_nodes++;
                }
            }
            break;
        }

        // Recuperar recursos: reaper de todos los hijos que ya terminaron
        int reaped = 0;
        int status;
        pid_t finished_pid;
        while ((finished_pid = waitpid(-1, &status, WNOHANG)) > 0) {
            reaped++;
            active_procs--;
            for (int i = 0; i < total_tasks; i++) {
                if (tasks[i].status != RUNNING || tasks[i].pid != finished_pid) continue;

                Message msg;
                int ok = WIFEXITED(status) && WEXITSTATUS(status) == 0 && leer_mensaje(tasks[i].rfd, &msg) == 0;
                close(tasks[i].rfd);

                if (ok) {
                    tasks[i].status = COMPLETED;
                    completed_nodes++;
                    snprintf(tasks[i].out_msg, sizeof(tasks[i].out_msg), "%s", msg.text);
                    printf(" Completado: %s. %s\n", tasks[i].name, msg.text);

                    // Propagar el mensaje a las actividades dependientes y marcar la dependencia cumplida
                    for (int j = 0; j < total_tasks; j++) {
                        for (int d = 0; d < tasks[j].num_deps; d++) {
                            if (strcmp(tasks[j].deps[d], tasks[i].id) == 0) {
                                tasks[j].deps_met++;
                                tasks[j].src[tasks[j].num_src++] = i;
                            }
                        }
                    }
                } else {
                    tasks[i].status = FAILED;
                    aborted_nodes++;
                    if (WIFSIGNALED(status))
                        printf(" Error en: %s (terminado por senal %d)\n", tasks[i].name, WTERMSIG(status));
                    else
                        printf(" Error en: %s\n", tasks[i].name);
                    abort_branch(tasks, total_tasks, tasks[i].id, &aborted_nodes);
                }
                break;
            }
        }

        // Nadie termino aun: dormir hasta SIGCHLD o SIGINT (desbloqueo + espera atomicos)
        if (reaped == 0) sigsuspend(&original);
    }

    // Inspeccion de la Seremi: abortar TODO
    if (seremi_llegada) {
        printf("\n ¡Llegó la LEY Quieto todo el mundo! Abortando todas las actividades...\n");
        for (int i = 0; i < total_tasks; i++) {
            if (tasks[i].status == RUNNING) {
                kill(tasks[i].pid, SIGKILL);
                waitpid(tasks[i].pid, NULL, 0); // reapear: sin zombies
                close(tasks[i].rfd);
                tasks[i].status = ABORTED;
                aborted_nodes++;
            } else if (tasks[i].status == WAITING) {
                tasks[i].status = ABORTED;
                aborted_nodes++;
            }
        }
    }

    printf("\nFinalizado. (Completadas: %d, Abortadas: %d)\n", completed_nodes, aborted_nodes);
    free(tasks);
    return EXIT_SUCCESS;
}

/* tipos.h */
#ifndef TIPOS_H //para el caso en que no este definido el TIPOS_H, pues que se defina

#define TIPOS_H //comienza el bloque de definicion

#include <sys/types.h>

#define MAX_TASKS 10000
#define MAX_DEPS 50

// Estados de una actividad
typedef enum { WAITING, RUNNING, COMPLETED, FAILED, ABORTED } Status;

// Estructura del nodo del Grafo (DAG)
typedef struct {
    char id[32];
    char name[128];
    int time_ms;
    char deps[MAX_DEPS][32];
    int num_deps;
    int deps_met;
    Status status;
    pid_t pid;

    int rfd;              // extremo de lectura del pipe propio de este hijo (solo valido en RUNNING)
    char out_msg[256];    // mensaje que esta actividad emitio al terminar (recibido por el pipe)
    int src[MAX_DEPS];    // indices de las actividades cuyos insumos ya llegaron a esta tarea
    int num_src;
} Task;

// Estructura para el paso de mensajes por Pipe
typedef struct {
    char id[32];
    char text[256];
} Message;

#endif //termina el bloque de definicion del TIPOS_H

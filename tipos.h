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
} Task;

// Estructura para el paso de mensajes por Pipe
typedef struct {
    char id[32];
    char text[256];
} Message;

#endif //termina el bloque de definicion del TIPOS_H
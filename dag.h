/* dag.h */
#ifndef DAG_H
#define DAG_H

#include "tipos.h"

// Lee el archivo plan.txt y llena el arreglo de tareas
int parse_plan(const char* filename, Task* tasks, int* total_tasks);

// Función recursiva para aislar errores y abortar ramas
void abort_branch(Task tasks[], int total_tasks, const char* failed_id, int* aborted_count);

#endif
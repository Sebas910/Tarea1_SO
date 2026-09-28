/* dag.c */
#include "dag.h"
#include "utilidad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void abort_branch(Task tasks[], int total_tasks, const char* failed_id, int* aborted_count) {
    for (int i = 0; i < total_tasks; i++) {
        for (int d = 0; d < tasks[i].num_deps; d++) {
            if (strcmp(tasks[i].deps[d], failed_id) == 0) {
                if (tasks[i].status == WAITING) {
                    tasks[i].status = ABORTED;
                    (*aborted_count)++;
                    printf("Abortando rama dependiente: %s (falto insumo de %s)\n", tasks[i].name, failed_id);
                    // Llamada recursiva para abortar en cadena
                    abort_branch(tasks, total_tasks, tasks[i].id, aborted_count);
                }
            }
        }
    }
}

int parse_plan(const char* filename, Task* tasks, int* total_tasks) { //Funcion parser para leer cada linea del archivo plan.txt
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo el archivo");
        return -1;
    }

    int count = 0;
    char line[512];

    while (fgets(line, sizeof(line), file) && count < MAX_TASKS) {
        char *id_str = line;
        char *name_str = strchr(id_str, ':');
        if (!name_str) continue; //por si acasongo plan.txt esta mal escrito
        *name_str++ = '\0';

        char *time_str = strchr(name_str, ':');
        if (!time_str) continue;
        *time_str++ = '\0'; //el diablo esta linea es de genios, primero establece el fin de una cadena y luego suma 1 para saltar a la siguiente

        char *deps_str = strchr(time_str, ':');
        if (deps_str) *deps_str++ = '\0';

        strcpy(tasks[count].id, trim(id_str)); //recordar que trim es una funcion definida en utilidad.c
        strcpy(tasks[count].name, trim(name_str));

        char *t_trim = trim(time_str);
        if (strlen(t_trim) == 0) {
            tasks[count].time_ms = 100 + rand() % 4901; 
        } else {
            tasks[count].time_ms = atoi(t_trim);
        }

        tasks[count].num_deps = 0;
        tasks[count].deps_met = 0;
        tasks[count].status = WAITING;

        if (deps_str) {
            char *dep = strtok(deps_str, ",\n");
            while (dep && tasks[count].num_deps < MAX_DEPS) {
                strcpy(tasks[count].deps[tasks[count].num_deps++], trim(dep)); //dificil de leer a primera vista, es una matriz donde la fila guarda la dependencia 
                dep = strtok(NULL, ",\n"); //pasa al siguiente valor que este separado por coma en la parte de las depedencias en plan.txt 
            }
        }
        count++;
    }
    fclose(file);
    *total_tasks = count;
    return 0;
}
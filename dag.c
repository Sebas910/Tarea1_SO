/* dag.c */
#include "dag.h"
#include "utilidad.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void abort_branch(Task tasks[], int total_tasks, const char* failed_id, int* aborted_count) {
    for (int i = 0; i < total_tasks; i++) { //recorrer todas las tareas 
        for (int d = 0; d < tasks[i].num_deps; d++) { // recorrer las dependencias
            if (strcmp(tasks[i].deps[d], failed_id) == 0) { //tarea i dependia del fallo?
                if (tasks[i].status == WAITING) {
                    tasks[i].status = ABORTED;
                    (*aborted_count)++;
                    printf("Abortando rama dependiente: %s (falto insumo de %s)\n", tasks[i].name, failed_id);
                    // Llamada recursiva para abortar en cadena
                    abort_branch(tasks, total_tasks, tasks[i].id, aborted_count);
                }
                break; 
            }
        }
    }
}

// Copia src en dest verificando que quepa. Devuelve 0 si cabe, -1 si habria desbordado.
static int copiar_seguro(char* dest, size_t size, const char* src) {
    size_t len = strlen(src);
    if (len >= size) return -1;
    memcpy(dest, src, len + 1);
    return 0;
}

int parse_plan(const char* filename, Task* tasks, int* total_tasks) { //Funcion parser para leer cada linea del archivo plan.txt
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Error abriendo el archivo");
        return -1;
    }

    int count = 0;
    int lineno = 0;
    char line[4096];

    while (fgets(line, sizeof(line), file)) {
        lineno++;

        // Linea mas larga que el buffer: se descarta completa para no leer la mitad como otra linea
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] != '\n' && !feof(file)) {
            fprintf(stderr, "Aviso: linea %d demasiado larga, se ignora.\n", lineno);
            int c;
            while ((c = fgetc(file)) != '\n' && c != EOF) { }
            continue;
        }

        if (*trim(line) == '\0') continue; // linea en blanco (trim tambien limpia \r de archivos de Windows, \n)

        //se busca el siguiente carácter ':' asi separando el plan.txt en 4 segmentos, id, nombre, tiempo y dependencias
        char *id_str = line;
        char *name_str = strchr(id_str, ':');
        if (!name_str) { fprintf(stderr, "Aviso: linea %d mal formada, se ignora.\n", lineno); continue; }
        *name_str++ = '\0';

        char *time_str = strchr(name_str, ':');
        if (!time_str) { fprintf(stderr, "Aviso: linea %d mal formada, se ignora.\n", lineno); continue; }
        *time_str++ = '\0'; // termina el nombre y avanza a lo que viene despues del segundo ':'

        char *deps_str = strchr(time_str, ':');
        if (deps_str) *deps_str++ = '\0';

        char *id_t = trim(id_str);
        char *name_t = trim(name_str);
        if (*id_t == '\0') { fprintf(stderr, "Aviso: linea %d sin ID, se ignora.\n", lineno); continue; }

        if (count >= MAX_TASKS) {
            fprintf(stderr, "Aviso: el plan supera MAX_TASKS (%d); se ignoran las actividades restantes.\n", MAX_TASKS);
            break;
        }

        for (int k = 0; k < count; k++) {
            if (strcmp(tasks[k].id, id_t) == 0) {
                fprintf(stderr, "Error: ID duplicado '%s' en la linea %d.\n", id_t, lineno);
                fclose(file);
                return -1;
            }
        }

        if (copiar_seguro(tasks[count].id, sizeof(tasks[count].id), id_t) < 0 ||
            copiar_seguro(tasks[count].name, sizeof(tasks[count].name), name_t) < 0) {
            fprintf(stderr, "Error: ID o nombre demasiado largo en la linea %d.\n", lineno);
            fclose(file);
            return -1;
        }

        char *t_trim = trim(time_str);
        if (strlen(t_trim) == 0) {
            tasks[count].time_ms = 100 + rand() % 4901; // aleatorio entre 100 y 5000
        } else {
            tasks[count].time_ms = atoi(t_trim);
            if (tasks[count].time_ms < 0) tasks[count].time_ms = 0;
        }

        tasks[count].num_deps = 0;
        tasks[count].deps_met = 0;
        tasks[count].num_src = 0;
        tasks[count].status = WAITING;

        if (deps_str) {
            char *dep = strtok(deps_str, ",\r\n");
            while (dep) {
                char *d = trim(dep);
                if (*d != '\0') { // ignora tokens vacios (espacios o \r sobrantes)
                    if (tasks[count].num_deps >= MAX_DEPS ||
                        copiar_seguro(tasks[count].deps[tasks[count].num_deps], sizeof(tasks[count].deps[0]), d) < 0) {
                        fprintf(stderr, "Error: demasiadas dependencias o ID demasiado largo en la linea %d.\n", lineno);
                        fclose(file);
                        return -1;
                    }
                    tasks[count].num_deps++;
                }
                dep = strtok(NULL, ",\r\n"); //pasa al siguiente valor separado por coma
            }
        }
        count++;
    }
    fclose(file);
    *total_tasks = count;
    return 0;
}

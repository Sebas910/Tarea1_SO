#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>

//Incluyendo los archivos con las funciones programadas en los otros archivos .c
#include "tipos.h"
#include "dag.h"
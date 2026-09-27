/* utilidad.c */
#include "utilidad.h"
#include <ctype.h>
#include <string.h>

char* trim(char* str) {
    while (isspace((unsigned char)*str)) str++; //unsigne char para transformar *str en un valor entre 0 y 255
    if (*str == 0) return str; //en caso de que str se encuentre vacio (no haya nada en esa direccion de memoria)
    char* end = str + strlen(str) - 1; //genera una variable que apunta al ultimo caracter de str
    while (end > str && isspace((unsigned char)*end)) end--; // Aqui somos bakanes e iteramos hasta que end apunta al ultimo caracter que no sea un espacio
    end[1] = '\0'; // le decimo, el weso amigo el siguiente entonce es el final de la word tu me entiende 
    return str; // devolvemos el string sin los espacios inncesarios
}  
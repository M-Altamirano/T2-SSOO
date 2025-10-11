#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include <string.h> //para strcmp
#include <stdbool.h> // bool, true, false
#include "os_memory_API.h"


// funciones generales

void mount_memory(char** global_path, char* memory_path) {
    *global_path = malloc(strlen(memory_path) + 1);
    strcpy(*global_path, memory_path);
}

// // funciones procesos

// // funciones archivos
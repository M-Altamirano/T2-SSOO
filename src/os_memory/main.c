#include <stdlib.h>
#include "../os_memory_API/os_memory_API.h"
#include <stdio.h>


int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Uso: %s <mem.bin>\n", argv[0]);
        return 1;
    }
    char* mem_path = NULL;
    mount_memory(&mem_path, argv[1]);

    list_processes();
    int libres = processes_slots();
    printf("PCB free slots: %d\n", libres);
    frame_bitmap_status();
    // Ejemplo: listar archivos de PID 10
    list_files(10);

    free(mem_path);
    return 0;
}
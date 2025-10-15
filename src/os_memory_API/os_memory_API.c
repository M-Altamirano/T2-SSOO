#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include <string.h> //para strcmp
#include <stdbool.h> // bool, true, false
#include "os_memory_API.h"


// funciones generales
static void print_processes_in_terminal(Memory* mem) {
    for (int i = 0; i < 32; i++) {
        if (!mem->processes[i]->state) continue;
        char buffer[15];
        ProcessEntry* current = mem->processes[i];
        for (int j = 0; j < 14; j++) {
            buffer[j] = current->name[j];
        }
        buffer[14] = '\0';
        printf("%u %s\n", current->id, buffer);
    }
}

static void print_processes_in_file(Memory* mem, FILE* output) {
    for (int i = 0; i < 32; i++) {
        if (!mem->processes[i]->state) continue;
        char buffer[15];
        ProcessEntry* current = mem->processes[i];
        for (int j = 0; j < 14; j++) {
            buffer[j] = current->name[j];
        }
        buffer[14] = '\0';
        fprintf(output, "%u %s\n", current->id, buffer);
    }
}

static void list_files_terminal(int process_id, Memory* mem) {
    uint8_t id = (uint8_t) process_id;
    int index = 0;
    while (mem->processes[index]->id != id && index < 32) index++;
    if (index == 32) return;
    ProcessEntry* current_process = mem->processes[index];
    for (int i = 0; i < 10; i++) {
        ArchiveEntry current_arch = current_process->archive_table[i];
        if (!current_arch.valid) continue;
        char buffer[15];
        for (int j = 0; j < 14; j++) {
            buffer[j] = current_arch.name[j];
        }
        buffer[14] = '\0';
        uint32_t virtual = (uint32_t)current_arch.VPN << 15 | (uint32_t) current_arch.offset;
        printf("%03X %lu %08X %s\n", current_arch.VPN, (unsigned long) current_arch.size, virtual, buffer);
    }
}

static void list_files_file(int process_id, Memory* mem, FILE* output) {
    uint8_t id = (uint8_t) process_id;
    int index = 0;
    while (mem->processes[index]->id != id && index < 32) index++;
    if (index == 32) return;
    ProcessEntry* current_process = mem->processes[index];
    for (int i = 0; i < 10; i++) {
        ArchiveEntry current_arch = current_process->archive_table[i];
        if (!current_arch.valid) continue;
        char buffer[15];
        for (int j = 0; j < 14; j++) {
            buffer[j] = current_arch.name[j];
        }
        buffer[14] = '\0';
        uint32_t virtual = (uint32_t)current_arch.VPN << 15 | (uint32_t) current_arch.offset;
        fprintf(output, "%03X %lu %08X %s\n", current_arch.VPN, (unsigned long) current_arch.size, virtual, buffer);
    }
}


void mount_memory(char** global_path, char* memory_path) {
    *global_path = malloc(strlen(memory_path) + 1);
    strcpy(*global_path, memory_path);
}

Memory* read_memory(char* global_path) {
    FILE* mem = fopen(global_path, "rb");
    Memory* memory = calloc(1, sizeof(Memory));
    for (int i = 0; i < 32; i++) {
        memory->processes[i] = calloc(1, sizeof(ProcessEntry));
        fread(memory->processes[i], sizeof(ProcessEntry), 1, mem);
    }
    for (int i = 0; i < 65536; i++) {
        fread(&(memory->InvertedPageTable[i]), sizeof(InvertedPageTableEntry), 1, mem);
        if (memory->InvertedPageTable[i].valid) {
            SET_BIT(frame_bitmap, i);
        }
    }
    return memory;
}

void list_processes(Memory* mem) {
    print_processes_in_terminal(mem);
    // FILE* output = fopen("test.txt", "w");
    // print_processes_in_file(mem, output);
    // fclose(output);
}

int processes_slots(Memory* mem) {
    int free_slots = 0;
    for (int i = 0; i < 32; i++) if (!mem->processes[i]->state) free_slots++;
    return free_slots;
}

void list_files(int process_id, Memory* mem) {
    list_files_terminal(process_id, mem);
    // FILE* output = fopen("test.txt", "w");
    // list_files_file(process_id, mem, output);
    // fclose(output);
}

void frame_bitmap_status() {
    unsigned int used = 0;
    for (unsigned int i = 0; i < BITMAP_SIZE_BYTES * 8; i++) if (CHECK_BIT(frame_bitmap, i)) used ++;
    printf("USADOS: %u LIBRES: %u\n", used, BITMAP_SIZE_BYTES*8 - used);
}

// // funciones procesos

// // funciones archivos
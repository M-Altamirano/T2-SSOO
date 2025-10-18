#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include <string.h> //para strcmp
#include <stdbool.h> // bool, true, false
#include "os_memory_API.h"


 // Funciones estáticas auxiliares 

static FILE* open_mem_rb(void) {

    if (!mem_path) {
        fprintf(stderr, "[os_memory_API] Error: memoria no montada. Llama a mount_memory() primero.\n");
        return NULL;
    }

    FILE* f = fopen(mem_path, "rb");

    if (!f) {
        perror("[os_memory_API] fopen");
    }
    return f;
}

// Abrir memoria en modo rb+
static FILE* open_mem_rbplus(void) {

    if (!mem_path) {
        fprintf(stderr, "[os_memory_API] Error: memoria no montada. Llama a mount_memory() primero.\n");
        return NULL;
    }

    FILE* f = fopen(mem_path, "rb+");

    if (!f) perror("[os_memory_API] fopen(rb+)");
    return f;
}

static inline long pcb_entry_offset(int idx) {
    return OFFSET_PCB + (long)idx * PCB_ENTRY_SIZE_BYTES;
}


static void read_name_14(FILE* f, char out15[15]) {
    unsigned char raw[14];
    size_t r = fread(raw, 1, 14, f);
    if (r != 14) {
        // Si falla, llenamos con 0 para evitar usar basura
        memset(raw, 0, 14);
    }
    memcpy(out15, raw, 14);
    out15[14] = '\0'; // null-terminate obligado por enunciado
}

static void write_name_14(FILE* f, const char* in) {
    unsigned char raw[14];
    memset(raw, 0, 14);
    if (in) {
        size_t n = strlen(in);
        if (n > 14) n = 14;
        memcpy(raw, in, n);
    }
    fwrite(raw, 1, 14, f);
}

static uint64_t read_u40_le(FILE* f) {
    // Lee 5 bytes little-endian (40 bits)
    unsigned char b[5];
    if (fread(b, 1, 5, f) != 5) return 0;
    uint64_t v = ((uint64_t)b[0])
               | ((uint64_t)b[1] << 8)
               | ((uint64_t)b[2] << 16)
               | ((uint64_t)b[3] << 24)
               | ((uint64_t)b[4] << 32);
    return v;
}

static uint32_t read_u32_le(FILE* f) {
    unsigned char b[4];
    if (fread(b, 1, 4, f) != 4) return 0;
    uint32_t v = ((uint32_t)b[0])
               | ((uint32_t)b[1] << 8)
               | ((uint32_t)b[2] << 16)
               | ((uint32_t)b[3] << 24);
    return v;
}


// Decodifica una entrada de la IPT desde el archivo f donde bit 0 es valid, 1 a 10 es PID, 11 a 23 VPN
static uint32_t ipt_read_entry(FILE* f, int pfn, uint8_t* valid, uint16_t* pid10, uint16_t* vpn13) {
    unsigned char b[3];
    if (fseek(f, OFFSET_IPT + (long)pfn * 3L, SEEK_SET) != 0) return 0;
    if (fread(b, 1, 3, f) != 3) return 0;
    uint32_t v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16);
    if (valid) *valid = (uint8_t)(v & 0x1);
    if (pid10) *pid10 = (uint16_t)((v >> 1) & 0x3FF);
    if (vpn13) *vpn13 = (uint16_t)((v >> 11) & 0x1FFF);
    return v;
}


static void ipt_write_entry_raw(FILE* f, int pfn, uint32_t raw24) {
    unsigned char b[3];
    b[0] = (unsigned char)(raw24 & 0xFF);
    b[1] = (unsigned char)((raw24 >> 8) & 0xFF);
    b[2] = (unsigned char)((raw24 >> 16) & 0xFF);
    fseek(f, OFFSET_IPT + (long)pfn * 3L, SEEK_SET);
    fwrite(b, 1, 3, f);
}


static void ipt_invalidate_entry(FILE* f, int pfn) {
    // Poner los 3 bytes en cero (valid=0 y limpia pid/vpn)
    ipt_write_entry_raw(f, pfn, 0);
}


// funciones generales
static void print_processes_in_terminal() {
    for (int i = 0; i < 32; i++) {
        if (!sim_memory->processes[i]->state) continue;
        char buffer[15];
        ProcessEntry* current = sim_memory->processes[i];
        for (int j = 0; j < 14; j++) {
            buffer[j] = current->name[j];
        }
        buffer[14] = '\0';
        printf("%u %s\n", current->id, buffer);
    }
}

static void print_processes_in_file(FILE* output) {
    for (int i = 0; i < 32; i++) {
        if (!sim_memory->processes[i]->state) continue;
        char buffer[15];
        ProcessEntry* current = sim_memory->processes[i];
        for (int j = 0; j < 14; j++) {
            buffer[j] = current->name[j];
        }
        buffer[14] = '\0';
        fprintf(output, "%u %s\n", current->id, buffer);
    }
}

static void list_files_terminal(int process_id) {
    uint8_t id = (uint8_t) process_id;
    int index = 0;
    while (sim_memory->processes[index]->id != id && index < 32) index++;
    if (index == 32) return;
    ProcessEntry* current_process = sim_memory->processes[index];
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

static void list_files_file(int process_id, FILE* output) {
    uint8_t id = (uint8_t) process_id;
    int index = 0;
    while (sim_memory->processes[index]->id != id && index < 32) index++;
    if (index == 32) return;
    ProcessEntry* current_process = sim_memory->processes[index];
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


void mount_memory(char* memory_path) {
    mem_path = malloc(strlen(memory_path) + 1);
    strcpy(mem_path, memory_path);
}

Memory* read_memory() {
    FILE* mem = fopen(mem_path, "rb");
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
    fclose(mem);
    return memory;
}

void list_processes() {
    print_processes_in_terminal();
    // FILE* output = fopen("test.txt", "w");
    // print_processes_in_file(mem, output);
    // fclose(output);
}

int processes_slots(Memory* mem) {
    int free_slots = 0;
    for (int i = 0; i < 32; i++) if (!mem->processes[i]->state) free_slots++;
    return free_slots;
}

void list_files(int process_id) {
    list_files_terminal(process_id);
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

// Busca el indice de PCB activo con process_id, -1 si no existe o error 
static int find_active_pcb_index(FILE* f, int process_id) {
    for (int i = 0; i < PCB_COUNT; i++) {
        if (fseek(f, pcb_entry_offset(i), SEEK_SET) != 0) return -1;
        uint8_t state = 0;
        if (fread(&state, 1, 1, f) != 1) return -1;

        char pname[15];
        read_name_14(f, pname);

        uint8_t pid = 0;
        if (fread(&pid, 1, 1, f) != 1) return -1;

        if (state == 0x01 && pid == (uint8_t)process_id) return i;
    }
    return -1;
}


// Busca el primer indice libre de la PCB (state == 0x00), -1 si no hay o error
static int find_free_pcb_index(FILE* f) {
    for (int i = 0; i < PCB_COUNT; i++) {
        if (fseek(f, pcb_entry_offset(i), SEEK_SET) != 0) return -1;
        uint8_t state = 0;
        if (fread(&state, 1, 1, f) != 1) return -1;
        if (state == 0x00) return i;
    }
    return -1;
}


//  Escribe una PCB nueva donde state = 1, name[14], id y tabla de archivos en 0
static int write_new_pcb(FILE* f, int idx, int process_id, const char* process_name) {
    long off = pcb_entry_offset(idx);
    if (fseek(f, off, SEEK_SET) != 0) return -1;

    // state
    uint8_t state = 0x01;
    if (fwrite(&state, 1, 1, f) != 1) return -1;

    // name (14 bytes sin '\0')
    write_name_14(f, process_name);

    // id (1 byte)
    uint8_t pid = (uint8_t)process_id;
    if (fwrite(&pid, 1, 1, f) != 1) return -1;

    // Tabla de archivos: 10 entradas * 24 B, todo a 0
    unsigned char zeros[ARCHIVE_ENTRY_SIZE];
    memset(zeros, 0, sizeof(zeros));
    for (int j = 0; j < ARCHIVE_ENTRIES_PER_PROCESS; j++) {
        if (fwrite(zeros, 1, ARCHIVE_ENTRY_SIZE, f) != ARCHIVE_ENTRY_SIZE) return -1;
    }
    // Relleno hasta completar 256 B si hiciera falta (ya estamos exactamente en 256)
    return 0;
}



// Limpia completamente una entrada de PCB (pone todo (256) a 0)
static int zero_pcb(FILE* f, int idx) {
    long off = pcb_entry_offset(idx);
    if (fseek(f, off, SEEK_SET) != 0) return -1;
    unsigned char zeros[PCB_ENTRY_SIZE_BYTES];
    memset(zeros, 0, sizeof(zeros));
    if (fwrite(zeros, 1, sizeof(zeros), f) != sizeof(zeros)) return -1;
    return 0;
}




//// 

int start_process(int process_id, char* process_name) {
    FILE* f = open_mem_rbplus();
    if (!f) return -1;

    // Verificar que no exista ya un PCB activo con ese id
    int existing = find_active_pcb_index(f, process_id);
    if (existing >= 0) { fclose(f); return -1; }

    // Buscar slot libre
    int free_idx = find_free_pcb_index(f);
    if (free_idx < 0) { fclose(f); return -1; }

    // Escribir PCB nuevo
    int ok = write_new_pcb(f, free_idx, process_id, process_name);
    fflush(f);
    fclose(f);
    return (ok == 0) ? 0 : -1;
}


// libera todos los pfn pertenecientes a pid8 en IPT y bitma. Retorna la cantidad de frames liberados
static int free_pid_frames(FILE* f, uint8_t pid8) {
    // 1) Leer bitmap actual
    if (fseek(f, OFFSET_BITMAP, SEEK_SET) != 0) return -1;
    if (fread(frame_bitmap, 1, BITMAP_SIZE_BYTES, f) != BITMAP_SIZE_BYTES) return -1;

    int freed = 0;

    // 2) Recorrer IPT completa (65536 entradas x 3 bytes)
    for (int pfn = 0; pfn < FRAMES_TOTAL; pfn++) {
        uint8_t valid = 0;
        uint16_t pid10 = 0, vpn13 = 0;
        uint32_t raw = ipt_read_entry(f, pfn, &valid, &pid10, &vpn13);
        // Si raw==0 y valid==0, ya está invalidado; sigue
        if (!valid) continue;

        // Comparar pid: IPT tiene 10 bits, nosotros usamos 8
        uint8_t pid_entry = (uint8_t)(pid10 & 0xFF);
        if (pid_entry == pid8) {
            // invalidar IPT
            ipt_invalidate_entry(f, pfn);
            // limpiar bit en bitmap
            CLEAR_BIT(frame_bitmap, pfn);
            freed++;
        }
    }

    // 3) Guardar bitmap
    if (fseek(f, OFFSET_BITMAP, SEEK_SET) != 0) return -1;
    if (fwrite(frame_bitmap, 1, BITMAP_SIZE_BYTES, f) != BITMAP_SIZE_BYTES) return -1;

    return freed;
}


int finish_process(int process_id) {
    FILE* f = open_mem_rbplus();
    if (!f) return -1;

    // Hallar PCB activo
    int idx = find_active_pcb_index(f, process_id);
    if (idx < 0) { fclose(f); return -1; }

    // Liberar toda la memoria (frames) asociada al pid en IPT/bitmap
    uint8_t pid8 = (uint8_t)process_id;
    int freed = free_pid_frames(f, pid8);
    if (freed < 0) { fclose(f); return -1; }

    // Borrar PCB (poner en 0 los 256 bytes)
    if (zero_pcb(f, idx) != 0) { fclose(f); return -1; }

    fflush(f);
    fclose(f);
    return 0;
}




// Como se “terminan todos” --> poner a cero TODA la IPT y TODO el bitmap, y luego limpiar todas las entradas de PCB activas. 
// Devolvemos cuántos procesos cerramos
int clear_all_processes(void) {

    FILE* f = open_mem_rbplus(); 
    if (!f) return -1;

    
    int closed = 0; // Contar procesos activos

    for (int i = 0; i < PCB_COUNT; i++) {
        if (fseek(f, pcb_entry_offset(i), SEEK_SET) != 0) { fclose(f); return -1; }
        uint8_t state = 0;
        if (fread(&state, 1, 1, f) != 1) {
            fclose(f);
            return -1;
        }
        if (state == 0x01) closed++;
    }

    // Limpiar tabla de PCBs completa
    if (fseek(f, OFFSET_PCB, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }
    unsigned char zeros_pcb[PCB_TABLE_SIZE];
    memset(zeros_pcb, 0, sizeof(zeros_pcb));
    if (fwrite(zeros_pcb, 1, sizeof(zeros_pcb), f) != sizeof(zeros_pcb)) {
        fclose(f);
        return -1;
    }


    // Limpiar IPT completa (192 KiB a cero)
    if (fseek(f, OFFSET_IPT, SEEK_SET) != 0) {
        fclose(f);
        return -1;
    }

    unsigned char zeros_ipt[4096]; // buffer para llenar a bloques
    memset(zeros_ipt, 0, sizeof(zeros_ipt));
    size_t remaining = IPT_SIZE_BYTES;

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(zeros_ipt) ? sizeof(zeros_ipt) : remaining;
        if (fwrite(zeros_ipt, 1, chunk, f) != chunk) { fclose(f); return -1; }
        remaining -= chunk;
    }


    // Limpiar bitmap completo
    memset(frame_bitmap, 0, BITMAP_SIZE_BYTES);
    if (fseek(f, OFFSET_BITMAP, SEEK_SET) != 0) { fclose(f); return -1; }
    if (fwrite(frame_bitmap, 1, BITMAP_SIZE_BYTES, f) != BITMAP_SIZE_BYTES) { fclose(f); return -1; }

    fflush(f);
    fclose(f);
    return closed;
}

int file_table_slots(int process_id) {
    FILE* f = open_mem_rb();
    if (!f) return -1;

    int idx = find_active_pcb_index(f, process_id);
    if (idx < 0) { fclose(f); return -1; }

    // Ir a la tabla de archivos del PCB encontrado
    long proc_off = pcb_entry_offset(idx);
    long arch_table_off = proc_off + 1 /*state*/ + 14 /*name*/ + 1 /*id*/;

    int free_slots = 0;
    for (int j = 0; j < ARCHIVE_ENTRIES_PER_PROCESS; j++) {
        long entry_off = arch_table_off + (long)j * ARCHIVE_ENTRY_SIZE;
        if (fseek(f, entry_off, SEEK_SET) != 0) { fclose(f); return -1; }
        uint8_t valid = 0;
        if (fread(&valid, 1, 1, f) != 1) { fclose(f); return -1; }
        if (valid != 0x01) free_slots++;
    }

    fclose(f);
    return free_slots;
}


// // funciones archivos

osmFile* open_file(int process_id, char* file_name, char mode) {
    int index = 0;
    while (index < 32 && (uint8_t) (0xFF & sim_memory->processes[index]->id) != (uint8_t) process_id) index++;
    if (index == 32) return NULL;
    ProcessEntry* process = sim_memory->processes[index];
    switch (mode)
    {
    case 'r':
        index = 0;
        while (index < 10) {
            char buffer[15];
            ArchiveEntry current_archive = process->archive_table[index];
            for (int i = 0; i < 14; i++) {
                buffer[i] = current_archive.name[i];
            }
            buffer[15] = '\0';
            if (strcmp(file_name, buffer) == 0) break;
            index++;
        }
        if (index == 10) return NULL;
        ArchiveEntry current_archive = process->archive_table[index];
        osmFile* file = calloc(1, sizeof(osmFile));
        file->VPN = current_archive.VPN;
        file->offset = current_archive.offset;
        file->size = current_archive.size;
        file->pid = (uint8_t) process_id;
        for (int i = 0; i < 14; i++) file->name[i] = current_archive.name[i];
        return file;
        break;
    
    case 'w':
    index = 0;
        while (index < 10) {
            char buffer[15];
            ArchiveEntry current_archive = process->archive_table[index];
            for (int i = 0; i < 14; i++) {
                buffer[i] = current_archive.name[i];
            }
            buffer[15] = '\0';
            if (strcmp(file_name, buffer) == 0) return NULL;
            index++;
        }
        if (index == 10) {
            osmFile* file = calloc(1, sizeof(osmFile));
            file->VPN = current_archive.VPN; // ACA ÑA CONCHA DE TU ,ADRE
            file->offset = current_archive.offset;
            file->size = 0;
            file->buffer = calloc(32*1024, sizeof(char));
            file->buffer_capacity = 32*1024;
            file->buffer_size = 0;
            file->frames = 1;
            for (int i = 0; i < 14; i++) file->name[i] = current_archive.name[i];
            return file;
        }
        break;
    
    default:
        break;
    }
}

int read_file(osmFile* file_desc, char* dest) {
    FILE* target_file = fopen(dest, "w");
    return (int) fwrite(file_desc->buffer, sizeof(char), file_desc->buffer_size, target_file);
}

int write_file(osmFile* file_desc, char* src) {
    FILE* target_file = fopen(src, "r");
    write_on_osmFile(file_desc, target_file);
    FILE* mem = fopen(mem_path, "rb");
    long dir = 0;
    bool found = false;
    while (!found) {
        InvertedPageTableEntry current = sim_memory->InvertedPageTable[dir];
        if (current.pid == file_desc->pid && current.VPN == file_desc->VPN) {
            found = true;
        }
        else dir++;
    }
    dir = dir << 15 | file_desc->offset;
    fseek(mem, (long) (16 + 3*64) * 1024 + dir, SEEK_SET);
    int bytes_read = (int) fwrite(file_desc->buffer, sizeof(char), file_desc->buffer_size, mem);
    fclose(mem);
    return bytes_read;

}

void delete_file(int process_id, char* file_name) {
    osmFile* file = open_file(process_id, file_name, "r");
    for (size_t i = 0; i < file->buffer_size; i++) file->buffer[i] = 0;
    long dir = 0;
    bool found = false;
    while (!found) {
        InvertedPageTableEntry current = sim_memory->InvertedPageTable[dir];
        if (current.pid == file->pid && current.VPN == file->VPN) {
            found = true;
        }
        else dir++;
    }
    dir = dir << 15 | file->offset;
    FILE* mem = fopen(mem_path, "rb");
    fseek(mem, (long) (16 + 3*64) * 1024 + dir, SEEK_SET);
    for (size_t i = 0; i < file->buffer_size; i++) file->buffer[i] = 0;
    int bytes_read = (int) fwrite(file->buffer, sizeof(char), file->buffer_size, mem);
    dir = dir & 0b111111111111000000000000000;
    fseek(mem, (long) (16 + 3*64) * 1024 + dir, SEEK_SET);
    char checker_buffer[32*1024];
    fwrite(mem, sizeof(char), 32*1024, checker_buffer);
    bool free_frame = true;
    for (int i = 0; i < 32*1024; i++) {
        if (checker_buffer[i] != 0) {
            free_frame = false;
            break;
        }
    }
    if (free_frame) CLEAR_BIT(frame_bitmap, dir >> 15);
    uint8_t cleared_frames = 0;
    do {
        CLEAR_BIT(frame_bitmap, (dir >> 15) + cleared_frames);
        cleared_frames++;
    }
    while (cleared_frames < file->frames);
    cleared_frames = 0;
    do {
        InvertedPageTableEntry current = sim_memory->InvertedPageTable[(dir >> 15) + cleared_frames];
        current.pid = 0;
        current.valid = 0;
        current.VPN = 0;
        cleared_frames++;
    }
    while (cleared_frames < file->frames);
}
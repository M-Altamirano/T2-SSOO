#include <stdio.h>	// FILE, fopen, fclose, etc.
#include <stdlib.h> // malloc, calloc, free, etc
#include <string.h> //para strcmp
#include <stdbool.h> // bool, true, false
#include "os_memory_API.h"

#include <inttypes.h> // PRIu64


#define PCB_ENTRY_SIZE_BYTES 256
#define PCB_COUNT 32
#define PCB_TABLE_SIZE (PCB_ENTRY_SIZE_BYTES * PCB_COUNT)

#define IPT_SIZE_BYTES (192 * 1024) // 192 Kb
#define FRAMES_TOTAL 65536 // 8 KiB * 8 = 65536 bits
#define FRAME_SIZE_BYTES (32 * 1024) // 32 Kb

#define OFFSET_PCB 0L
#define OFFSET_IPT (OFFSET_PCB + PCB_TABLE_SIZE)
#define OFFSET_BITMAP (OFFSET_IPT + IPT_SIZE_BYTES)
#define OFFSET_DATA (OFFSET_BITMAP + BITMAP_SIZE_BYTES)

#define ARCHIVE_ENTRY_SIZE 24
#define ARCHIVE_ENTRIES_PER_PROCESS 10



 // Variables estáticas 
static char* s_memory_path = NULL; // path interno de la librería
uint8_t frame_bitmap[BITMAP_SIZE_BYTES] = {0};



 // Funciones estáticas auxiliares 

static FILE* open_mem_rb(void) {

    if (!s_memory_path) {
        fprintf(stderr, "[os_memory_API] Error: memoria no montada. Llama a mount_memory() primero.\n");
        return NULL;
    }

    FILE* f = fopen(s_memory_path, "rb");

    if (!f) {
        perror("[os_memory_API] fopen");
    }
    return f;
}

// Abrir memoria en modo rb+
static FILE* open_mem_rbplus(void) {

    if (!s_memory_path) {
        fprintf(stderr, "[os_memory_API] Error: memoria no montada. Llama a mount_memory() primero.\n");
        return NULL;
    }

    FILE* f = fopen(s_memory_path, "rb+");

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




//////////////////////////////////////  funciones generales

void mount_memory(char** global_path, char* memory_path) {
    
    if (global_path) { // Usamos el path global
        *global_path = (char*)malloc(strlen(memory_path) + 1);
        if (*global_path) strcpy(*global_path, memory_path); // guardar el path global 
    }

    
    if (s_memory_path) { 
        free(s_memory_path);
        s_memory_path = NULL;
    }
    s_memory_path = (char*)malloc(strlen(memory_path) + 1); // guardar el path interno
    if (s_memory_path) strcpy(s_memory_path, memory_path);
}


// imprimimos en el orden para cada PCB activo
void list_processes(void) { 

    FILE* f = open_mem_rb();
    if (!f) return;

    for (int i = 0; i < PCB_COUNT; i++) {
        if (fseek(f, pcb_entry_offset(i), SEEK_SET) != 0) {
            perror("[os_memory_API] fseek(list_processes)");
            fclose(f);
            return;
        }


        uint8_t state = 0;
        if (fread(&state, 1, 1, f) != 1) {
            perror("[os_memory_API] fread(state)");
            fclose(f);
            return;
        }


        char pname[15];
        read_name_14(f, pname);


        uint8_t pid = 0;
        if (fread(&pid, 1, 1, f) != 1) {
            perror("[os_memory_API] fread(pid)");
            fclose(f);
            return;
        }


        if (state == 0x01) {
            printf("%u %s\n", (unsigned)pid, pname);
        }
        
    }

    fclose(f);
}


// retorna la cantidad de slots libres ( state == 0x00) para los procesos en la tabla PCB
int processes_slots(void) {
    FILE* f = open_mem_rb();
    if (!f) return -1; // msg de error

    int free_count = 0;

    for (int i = 0; i < PCB_COUNT; i++) {
        if (fseek(f, pcb_entry_offset(i), SEEK_SET) != 0) {
            perror("[os_memory_API] fseek(processes_slots)");
            fclose(f);
            return -1;
        }
        uint8_t state = 0;
        if (fread(&state, 1, 1, f) != 1) {
            perror("[os_memory_API] fread(state)");
            fclose(f);
            return -1;
        }
        if (state == 0x00) free_count++;
    }

    fclose(f);
    return free_count;
}




// Para cada archivo valido del proceso con id = process_id, imprime:
// <VPN_hex> <FILE_SIZE_decimal> <VADDR_hex> <FILE_NAME>" , 

// !!!!! vpn y vaddr en hexa y FILE_SIZE en decimal !!!!!

void list_files(int process_id) {

    FILE* f = open_mem_rb();
    if (!f) return;

    // Buscamos PCB con state=1 y id == process_id
    long proc_off = -1;

    for (int i = 0; i < PCB_COUNT; i++) {
        if (fseek(f, pcb_entry_offset(i), SEEK_SET) != 0) {
            perror("[os_memory_API] fseek(list_files:pcb)");
            fclose(f);
            return;
        }

        uint8_t state = 0;
        if (fread(&state, 1, 1, f) != 1) {
            perror("[os_memory_API] fread(state)");
            fclose(f);
            return;
        }

        char pname[15];
        read_name_14(f, pname);


        uint8_t pid = 0;
        if (fread(&pid, 1, 1, f) != 1) {
            perror("[os_memory_API] fread(pid)");
            fclose(f);
            return;
        }


        if (state == 0x01 && pid == (uint8_t)process_id) {
            proc_off = pcb_entry_offset(i);
            break;
        }
    }


    if (proc_off < 0) {
        // Proceso no encontrado/activo
        printf("[os_memory_API] Proceso con ID %d no encontrado o inactivo.\n", process_id); // msg de error
        fclose(f);
        return;
    }


    // Offset al inicio de la tabla de archivos de este PCB:
    long arch_table_off = proc_off + 1 /*state*/ + 14 /*name*/ + 1 /*id*/;


    for (int j = 0; j < ARCHIVE_ENTRIES_PER_PROCESS; j++) {
        long entry_off = arch_table_off + (long)j * ARCHIVE_ENTRY_SIZE;
        if (fseek(f, entry_off, SEEK_SET) != 0) {
            perror("[os_memory_API] fseek(list_files:entry)");
            fclose(f);
            return;
        }

        uint8_t valid = 0;
        if (fread(&valid, 1, 1, f) != 1) {
            perror("[os_memory_API] fread(valid)");
            fclose(f);
            return;
        }
        if (valid != 0x01) { // pass
            continue;
        }

        char fname[15];
        read_name_14(f, fname);

        uint64_t fsize = read_u40_le(f);
        uint32_t vaddr = read_u32_le(f);


        // Se extrae VPN y offset desde vaddr (5 bits altos no significativos)
        uint16_t vpn    = (uint16_t)((vaddr >> 15) & 0x0FFF);
        uint16_t offset = (uint16_t)(vaddr & 0x7FFF);
        (void)offset; // Para después


        // Formato enuncia2:
        // <VPN_hex> <FILE_SIZE_decimal> <DIRECCION_VIRTUAL_hex> <FILE_NAME>
        printf("0x%03X %" PRIu64 " 0x%08X %s\n", vpn, (uint64_t)fsize, vaddr, fname);
    }

    fclose(f);
}



// Printea los frames usados y libres en el bitmap de frames respectivamente.
// Lee los 8Kib del bitmap desde el archivo de memoria.
void frame_bitmap_status(void) {

    FILE* f = open_mem_rb();
    if (!f) return;

    if (fseek(f, OFFSET_BITMAP, SEEK_SET) != 0) {
        perror("[os_memory_API] fseek(frame_bitmap_status)");
        fclose(f);
        return;
    }

    size_t rd = fread(frame_bitmap, 1, BITMAP_SIZE_BYTES, f);

    if (rd != BITMAP_SIZE_BYTES) {
        fprintf(stderr, "[os_memory_API] Error: bitmap leído incompleto (%zu/%d bytes)\n",
                rd, BITMAP_SIZE_BYTES);
        fclose(f);
        return;
    }
    fclose(f);

    unsigned used = 0;
    for (unsigned i = 0; i < FRAMES_TOTAL; i++) {
        if (CHECK_BIT(frame_bitmap, i)) used++;
    }

    unsigned free_ = FRAMES_TOTAL - used;
    
    printf("USADOS: %u LIBRES: %u\n", used, free_);
}




//////////////////////////// funciones procesos



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
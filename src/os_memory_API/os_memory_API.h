#pragma once
#pragma pack(push, 1) 
#include <stdio.h>
#include <stdint.h>
#include "../osm_File/osm_File.h"

#define BITMAP_SIZE_BYTES (8192)

uint8_t frame_bitmap[BITMAP_SIZE_BYTES] = {0};

#define SET_BIT(frame_bitmap, pos) ((frame_bitmap)[(pos)/8] |= (1 << ((pos)%8)))

#define CLEAR_BIT(frame_bitmap, pos) ((frame_bitmap)[(pos)/8] &= ~(1 << ((pos)%8)))

#define CHECK_BIT(frame_bitmap, pos) ((frame_bitmap)[(pos)/8] & (1 << ((pos)%8)))

typedef struct arch_entry{
    uint8_t valid;
    char name[14];
    uint64_t size:40;
    uint8_t padding:5;
    uint16_t VPN:12;
    uint16_t offset:15;
} ArchiveEntry;

typedef struct process_entry {
    uint8_t state;
    char name[14];
    uint8_t id;
    ArchiveEntry archive_table[10];
} ProcessEntry;

typedef struct inverted_page_table_entry {
    uint8_t valid:1;
    uint16_t pid:10;
    uint8_t padding:1;
    uint16_t VPN:12;
} InvertedPageTableEntry;

/* ====== FUNCIONES GENERALES ====== */

// void mount_memory(char* memory_path);

// void list_processes();

// int processes_slots();

// void list_files(int process_id);

// void frame_bitmap_status();



/* ====== FUNCIONES PARA PROCESOS ====== */

// int start_process(int process_id, char* process_name);

// int finish_process(int process_id);

// int clear_all_processes();

// int file_table_slots(int process_id);


/* ====== FUNCIONES PARA ARCHIVOS ====== */

// osmFile* open_file(int process_id, char* file_name, char mode);

// int read_file(osmFile* file desc, char* dest);

// int write_file(osmFile* file desc, char* src);

// void delete_file(int process id, char* file name);

// void close_file(osmFile* file_desc);


/*====== BONUS =====*/

// int format_memory(char* memory path);

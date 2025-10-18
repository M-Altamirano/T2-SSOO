#pragma once
#pragma pack(push, 1) 
#include <stdio.h>
#include <stdint.h>
#include "../osm_File/osm_File.h"


#define BITMAP_SIZE_BYTES (8192)

extern uint8_t frame_bitmap[BITMAP_SIZE_BYTES];

#define SET_BIT(frame_bitmap, pos) ((frame_bitmap)[(pos)/8] |= (1 << ((pos)%8)))

#define CLEAR_BIT(frame_bitmap, pos) ((frame_bitmap)[(pos)/8] &= ~(1 << ((pos)%8)))

#define CHECK_BIT(frame_bitmap, pos) ((frame_bitmap)[(pos)/8] & (1 << ((pos)%8)))

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

char* mem_path;

Memory* sim_memory;

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

typedef struct memory {
    ProcessEntry* processes[32];
    InvertedPageTableEntry InvertedPageTable[65536];
} Memory;

/* ====== FUNCIONES GENERALES ====== */

void mount_memory(char* memory_path);

Memory* read_memory();

void list_processes();

int processes_slots();

void list_files(int process_id);

void frame_bitmap_status();



/* ====== FUNCIONES PARA PROCESOS ====== */

// int start_process(int process_id, char* process_name);

// int finish_process(int process_id);

// int clear_all_processes();

// int file_table_slots(int process_id);


/* ====== FUNCIONES PARA ARCHIVOS ====== */

osmFile* open_file(int process_id, char* file_name, char mode);

int read_file(osmFile* file_desc, char* dest);

int write_file(osmFile* file_desc, char* src);

void delete_file(int process_id, char* file_name);

// void close_file(osmFile* file_desc);


/*====== BONUS =====*/

// int format_memory(char* memory_path);

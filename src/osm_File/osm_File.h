#pragma once
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct osm_file {
    char name[15];
    uint8_t pid;
    uint64_t size:40;
    uint16_t VPN:12;
    uint16_t offset:15;
    char* buffer;
    size_t buffer_size;
    size_t buffer_capacity;
    size_t frames;
} osmFile;

void write_on_osmFile(
  osmFile* osm_file,
  FILE* target_file
);
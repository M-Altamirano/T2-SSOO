#pragma once
#include <inttypes.h>

typedef struct osm_file {
    char name[14];
    uint64_t size:40;
} osmFile;
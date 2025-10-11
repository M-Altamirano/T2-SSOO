#include <stdint.h>

// Convert to little-endian (no-op on little-endian systems)
uint16_t to_little_endian_16(uint16_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return value;  // Already little-endian
    #else
        return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
    #endif
}

uint32_t to_little_endian_32(uint32_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return value;
    #else
        return ((value & 0xFF) << 24) | 
               ((value & 0xFF00) << 8) |
               ((value >> 8) & 0xFF00) |
               ((value >> 24) & 0xFF);
    #endif
}

uint64_t to_little_endian_64(uint64_t value) {
    #if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return value;
    #else
        return ((value & 0xFF) << 56) |
               ((value & 0xFF00) << 40) |
               ((value & 0xFF0000) << 24) |
               ((value & 0xFF000000) << 8) |
               ((value >> 8) & 0xFF000000) |
               ((value >> 24) & 0xFF0000) |
               ((value >> 40) & 0xFF00) |
               ((value >> 56) & 0xFF);
    #endif
}
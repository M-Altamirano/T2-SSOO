#include <stdlib.h>
#include "../os_memory_API/os_memory_API.h"

uint8_t frame_bitmap[BITMAP_SIZE_BYTES] = {0};


int main(int argc, char const *argv[]) {

  // montar la memoria
  mount_memory((char *)argv[1]);
  
  Memory* sim_memory = read_memory(mem_path);

  list_processes();

  printf("slots: %d\n", processes_slots());

  list_files(198);

  frame_bitmap_status();

  // FILE* f = fopen("memfilled.bin", "rb");
  // if (!f) {
  //     perror("fopen");
  //     return 1;
  // }
  
  // FILE* out = fopen("output.txt", "w");

  // for (int j = 0; j < 32; j++) {
  //   unsigned char buffer[256];
  //   size_t bytes_read = fread(buffer, 1, sizeof(buffer), f);


  //   printf("Read %zu bytes:\n", bytes_read);
  //   for (size_t i = 0; i < bytes_read; i++) {
  //       fprintf(out, "%02X ", buffer[i]);  // print each byte in hex
  //       if ((i + 1) % 16 == 0) fprintf(out, "\n"); // break every 16 bytes
  //   }
  //   fprintf(out, "\n");
  // }
  // fclose(f);
  // fclose(out);


  free(mem_path);

  return 0;
}
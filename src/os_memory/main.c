#include <stdlib.h>
#include "../os_memory_API/os_memory_API.h"


uint8_t frame_bitmap[BITMAP_SIZE_BYTES] = {0};

char* mem_path;

int main(int argc, char const *argv[]) {

  ProcessEntry** processes = calloc(32, sizeof(ProcessEntry)); 
  // montar la memoria
  mount_memory(&mem_path, (char *)argv[1]);
  
  for (int i = 0; i < 32; i++) {
    ProcessEntry* current_process = processes[i];
    unsigned char PCB_buffer[16];
    size_t process_bytes = fread(PCB_buffer, 1, sizeof(PCB_buffer), mem_path);
    if (process_bytes != 16) {
      printf("error reading process");
      return 1;
    }
    current_process->state = PCB_buffer[0];
    for (int j = 1; j < 15; j++) {
      current_process->name[j-1] = PCB_buffer[j];
    }
    current_process->name[14] = '\0';
    current_process->id = PCB_buffer[15];
  }

  printf("size of arch entry: %zu\n", sizeof(ArchiveEntry));
  printf("size of process entry: %zu\n", sizeof(ProcessEntry));
  printf("size of IPT entry: %zu\n", sizeof(InvertedPageTableEntry));
  printf("path: %s\n", mem_path);

  free(mem_path);

  return 0;
}
#include "../os_memory_API/os_memory_API.h"

int main(int argc, char const *argv[]) {
  // montar la memoria
  // os_mount((char *)argv[1]);
  printf("size of arch entry: %zu\n", sizeof(ArchiveEntry));
  printf("size of process entry: %zu\n", sizeof(ProcessEntry));
  printf("size of IPT entry: %zu\n", sizeof(InvertedPageTableEntry));
  return 0;
}
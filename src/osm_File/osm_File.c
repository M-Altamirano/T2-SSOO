#include "osm_File.h"

void write_on_osmFile(
  osmFile* osm_file,
  FILE* target_file
) {
  if (!osm_file || !target_file) return;
  if (osm_file->buffer_size == osm_file->buffer_capacity) {
    osm_file->frames += 1;
    osm_file->buffer_capacity = osm_file->frames * 32 * 1024;
    char* new_arr = (char*)realloc(osm_file->buffer, osm_file->buffer_capacity * sizeof(char));
    if (!new_arr) return;
    osm_file->buffer= new_arr;
  }
  size_t bytes_to_read = osm_file->buffer_capacity - osm_file->buffer_size;
  size_t bytes_read = fread(&(osm_file->buffer[osm_file->buffer_size]), sizeof(char), bytes_to_read, target_file);
  if (bytes_to_read > bytes_read) osm_file->buffer_size += bytes_read;
  else {
    osm_file->buffer_size = osm_file->buffer_capacity;
    write_on_osmFile(osm_file, target_file);
 }
}
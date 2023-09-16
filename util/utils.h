#ifndef utils_h
#define utils_h

#include <time.h>

int path_exist(const char *path);

int make_path(const char *dir);

int is_folder(const char *path);

char** split_string(char* input);

int remove_dir(char *path);

int extract_tar(const char* tar_file, const char* extract_dir);

int create_tar(char *pathname, char *tarpath);

char* calculate_sha256(const char* file_path);

void timestamp_to_string(time_t timestamp, char *buffer, size_t buffer_size);

#endif
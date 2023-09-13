#ifndef utils_h
#define utils_h

int folder_exist(char *folder);

int make_path(const char *dir);

char** split_string(char* input);

int remove_dir(char *path);

int extract_tar(const char* tar_file, const char* extract_dir);

int create_tar(char *pathname, char *tarpath);

char* calculate_sha256(const char* file_path);

#endif
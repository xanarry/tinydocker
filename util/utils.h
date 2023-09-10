#ifndef utils_h
#define utils_h


int folder_exist(char *folder);

int mkpath(const char *dir);

char** split_string(char* input);

int remove_directory(const char *path);

#endif
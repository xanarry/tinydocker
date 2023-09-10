#include <sys/stat.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>

int folder_exist(char *folder) {
    struct stat sb;
    return (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode)) ? 1 : 0;
}

int mkpath(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!folder_exist(tmp) && mkdir(tmp, S_IRWXU) == -1) {
                return -1;
            }
            *p = '/';
        }
    }
    return mkdir(tmp, S_IRWXU);
}


char** split_string(char* input) {
    char** result = 0;
    size_t count = 0;
    char* tmp = input;
    char* last_comma = 0;
    char delim[2] = " ";

    /* Count how many elements will be in the array */
    while (*tmp) {
        if (delim[0] == *tmp) {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token */
    count += last_comma < (input + strlen(input) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;
    result = malloc(sizeof(char*) * count);

    if (result) {
        size_t idx  = 0;
        char* token = strtok(input, delim);

        while (token) {
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        *(result + idx) = 0;
    }
    return result;
}


int remove_directory(const char *path) {
    DIR *dir = opendir(path);
    if (dir == NULL) {
        perror("opendir");
        return -1;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char child_path[PATH_MAX];
        snprintf(child_path, sizeof(child_path), "%s/%s", path, entry->d_name);

        struct stat st;
        if (lstat(child_path, &st) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISDIR(st.st_mode)) {
            remove_directory(child_path);
        } else {
            if (unlink(child_path) == -1) {
                perror("unlink");
            }
        }
    }

    closedir(dir);

    if (rmdir(path) == -1) {
        perror("rmdir");
        return -1;
    }

    return 0;
}


#define _XOPEN_SOURCE 500
#include <sys/stat.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <ftw.h>
#include <openssl/evp.h>
#include <limits.h>
#include <sys/stat.h>
#include "../logger/log.h"



off_t filesize(const char *filename) {
    struct stat st;
    if (stat(filename, &st) != 0) {
        return 0;
    }
    return st.st_size;
}

#define BUFFER_SIZE 1024

char* calculate_sha256(const char* file_path) {
    FILE* file = fopen(file_path, "rb");
    if (!file) {
        log_error("无法打开文件: %s", file_path);
        return NULL;
    }

    EVP_MD_CTX* md_context = EVP_MD_CTX_new();
    if (!md_context) {
        log_error("无法创建哈希上下文");
        fclose(file);
        return NULL;
    }

    if (EVP_DigestInit_ex(md_context, EVP_sha256(), NULL) != 1) {
        log_error("无法初始化哈希算法");
        EVP_MD_CTX_free(md_context);
        fclose(file);
        return NULL;
    }

    unsigned char buffer[BUFFER_SIZE];
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) != 0) {
        if (EVP_DigestUpdate(md_context, buffer, bytes_read) != 1) {
            log_error("无法更新哈希值");
            EVP_MD_CTX_free(md_context);
            fclose(file);
            return NULL;
        }
    }

    unsigned char sha256_hash[EVP_MAX_MD_SIZE];
    unsigned int sha256_hash_len;

    if (EVP_DigestFinal_ex(md_context, sha256_hash, &sha256_hash_len) != 1) {
        log_error("无法计算哈希值");
        EVP_MD_CTX_free(md_context);
        fclose(file);
        return NULL;
    }

    EVP_MD_CTX_free(md_context);
    fclose(file);

    char* sha256_string = (char*)malloc(sha256_hash_len * 2 + 1);
    if (!sha256_string) {
        log_error("内存分配失败");
        return NULL;
    }

    for (int i = 0; i < sha256_hash_len; i++) {
        sprintf(&sha256_string[i * 2], "%02x", (unsigned int)sha256_hash[i]);
    }

    return sha256_string;
}


int folder_exist(char *folder) {
    struct stat sb;
    return (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode)) ? 1 : 0;
}


int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
    int rv = remove(fpath);
    if (rv) {
        perror(fpath);
    }
    return rv;
}

int remove_dir(char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}


int make_path(const char *dir) {
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
    return  folder_exist(tmp) ? 0 : mkdir(tmp, S_IRWXU);
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

int create_tar(char *pathname, char *tarpath) {
    char cmd[258] = {0};
    sprintf(cmd, "tar -cf %s %s", tarpath, pathname);
    log_info("%s", cmd);
    return system(cmd);
}


int extract_tar(const char* tar_file, const char* extract_dir) {
    char cmd[258] = {0};
    sprintf(cmd, "tar -xf %s -C %s", tar_file, extract_dir);
    log_info("%s", cmd);
    return system(cmd);
}


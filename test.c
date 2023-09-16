#define _GNU_SOURCE
#include <sys/mount.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ftw.h>
#include <openssl/evp.h>
#include <limits.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "logger/log.h"

#define TINYDOCKER_RUNTIME_DIR "/home/xanarry/tinydocker_runtime"


int path_exist(const char *path) {
    if (access(path, F_OK) == 0)
        return 1;
    return 0;
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
            if (!path_exist(tmp) && mkdir(tmp, S_IRWXU) == -1) {
                return -1;
            }
            *p = '/';
        }
    }
    return path_exist(tmp) ? 0 : mkdir(tmp, S_IRWXU);
}


struct container_info1 {
    char *container_id; //容器ID
    char *image; //容器镜像
    char *command; //容器运行的命令
    int created; //容器创建时间
    int status; //容器状态
    char *name; //容器名字
};


enum container_status {
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_EXITED
};

struct container_info {
    char container_id[128]; //容器ID
    char image[128]; //容器镜像
    char command[128]; //容器运行的命令
    char created[20]; //容器创建时间
    char status[10]; //容器状态
    char name[128]; //容器名字
};

void timestamp_to_string(time_t timestamp, char *buffer, size_t buffer_size) {
    struct tm *timeinfo;
    timeinfo = localtime(&timestamp);

    strftime(buffer, buffer_size, "%Y-%m-%d %H:%M:%S", timeinfo);
}


int write_container_info(char *container_id, char *image, char *command, int created, enum container_status status, char *name) {
    char container_info_dir[512] = {0};
    sprintf(container_info_dir, "%s/container_info", TINYDOCKER_RUNTIME_DIR);
    if (!path_exist(container_info_dir)) {
        make_path(container_info_dir);
    }
    
    strcat(strcat(container_info_dir, "/"), container_id);
    int fd = open(container_info_dir, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        log_error("failed to open file: %s", container_info_dir);
        return 1;
    }

    char str_created_time[20] = {0};
    timestamp_to_string(created, str_created_time, 20);
    char *str_status[] = {"RUNNING", "STOPPED", "EXITED"};

    // 使用额外128的缓冲记录结构信息
    ssize_t size = 128 + strlen(container_id) + strlen(image) + strlen(command) + strlen(str_created_time) + strlen(name);
    char *content = (char *) malloc(sizeof(char) * size);
    sprintf(content, "container_id=%s\nimage=%s\ncommand=%s\ncreated=%s\nstatus=%s\nname=%s\n", container_id, image, command, str_created_time, str_status[status], name);
    
    if (write(fd, content, strlen(content)) == -1) {
        perror("Failed to write to file");
        close(fd);
        return 1;
    }
    close(fd);
    log_info("write container info %s in to %s", content, container_info_dir);
    return 0;
}

int read_container_info(const char *container_info_file, struct container_info *info) {
    FILE *file = fopen(container_info_file, "r");
    if (file == NULL) {
        log_error("failed to open container info file: %s", container_info_file);
        return -1;
    }

    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        // Split the line into key and value
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (strcmp(key, "container_id") == 0) {
            strcpy(info->container_id, value);
        }
        if (strcmp(key, "image") == 0) {
            strcpy(info->image, value);
        }
        if (strcmp(key, "command") == 0) {
            strcpy(info->command, value);
        }
        if (strcmp(key, "created") == 0) {
            strcpy(info->created, value);
        }
        if (strcmp(key, "status") == 0) {
            strcpy(info->status, value);
        }
        if (strcmp(key, "name") == 0) {
            strcpy(info->name, value);
        }
    }
    fclose(file);
    return 0;
}

int list_containers_info(struct container_info *container_info_list) {
    char container_info_dir[512] = {0};
    sprintf(container_info_dir, "%s/container_info", TINYDOCKER_RUNTIME_DIR);
    DIR *dir = opendir(container_info_dir);
    if (dir == NULL) {
        log_error("failed to open directory: %s", container_info_dir);
        return -1;
    }

    int container_cnt = 0;
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".." directories
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Construct the absolute path of the file
        char file_path[PATH_MAX];
        snprintf(file_path, sizeof(file_path), "%s/%s", container_info_dir, entry->d_name);

        // Check if the entry is a file
        struct stat file_stat;
        if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            log_info("load container info from: %s", file_path);

            struct container_info info;
            int ret = read_container_info(file_path, &info);
            if (ret == -1) {
                log_info("failed to load container info from: %s", file_path);
                continue;
            }
            container_info_list[container_cnt++] = info;
        }
    }

    closedir(dir);
    return container_cnt;
}


int docker_ps(int list_all) {
    struct container_info info_list[128];
    int cnt = list_containers_info(info_list);
    char *titles[] = {"CONTAINER ID", "IMAGE", "COMMAND", "CREATED", "STATUS", "NAMES", NULL};
    int spans[6] =   {12,              5,       7,         7,         6,        5}; //记录每个字段输出的最大宽度, 与titles一一对应
    for (int i = 0; i < cnt; i++) {
        if (list_all == 0 && strcmp(info_list[i].status, "RUNNING") != 0) {
            continue;
        }
        ssize_t container_id_len = strlen(info_list[i].container_id);
        spans[0] = container_id_len > spans[0] ? container_id_len : spans[0];

        ssize_t image_len = strlen(info_list[i].image);
        spans[1] = image_len > spans[1] ? image_len : spans[1];

        ssize_t command_len = strlen(info_list[i].command);
        spans[2] = command_len > spans[2] ? command_len : spans[2];

        spans[3] = 19; //2023-12-12 12:12:12这样的形式, 固定19长度
        spans[4] = 7; //RUNING|STOPPING|EXITED, 最长7

        ssize_t name_len = strlen(info_list[i].name);
        spans[5] = name_len > spans[5] ? name_len : spans[5];
    }

    for (int i = 0; titles[i] != NULL; i++) {
        printf("%-*s\t", spans[i], titles[i]);
    }
    printf("\n");
    for (int i = 0; i < cnt; i++) {
        if (list_all == 0 && strcmp(info_list[i].status, "RUNNING") != 0) {
            continue;
        }
        printf("%-*s\t%-*s\t%-*s\t%-*s\t%-*s\t%-*s\n", spans[0], info_list[i].container_id, spans[1], info_list[i].image, \
         spans[2], info_list[i].command, spans[3], info_list[i].created, spans[4], info_list[i].status, spans[5], info_list[i].name);
    }
}





int main(int argc, char const *argv[]) {
    write_container_info("id10", "image1", "/bin/bash -lc 'jel'", 123231234, CONTAINER_RUNNING, "name");
    write_container_info("id40", "image2", "/bin/bassdfasdfh -lc 'jel'", 1234, CONTAINER_RUNNING, "name");
    write_container_info("id5", "imagesdfasdf3", "/bin/bash -lc 'jel'", 1234, 1, "name");
    docker_ps(1);
}

#include <limits.h>
#include <stdio.h>
#include <dirent.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../logger/log.h"
#include "../util/utils.h"
#include "../cmdparser/cmdparser.h"
#include "container.h"
#include "status_info.h"

struct container_info create_container_info(struct docker_run_arguments *args, int pid, enum container_status status, int created_timestamp) {
    /*
    int pid; //容器进程ID;
    int detach//
    char container_id[128]; //容器ID
    char image[128]; //容器镜像
    char command[512]; //容器运行的命令
    char created[20]; //容器创建时间
    char status[10]; //容器状态
    char name[128]; //容器名字
    int volume_cnt; //卷挂载数量
    char volumes[128][1024]; //卷挂载信息
    */
    struct container_info info;
    char *str_status[] = {"RUNNING", "STOPPED", "EXITED"};

    info.pid = pid;
    info.detach = args->detach;
    sprintf(info.container_id, "%d", created_timestamp);
    strcpy(info.image, args->image);
    strcpy(info.command, args->container_argv[0]);
    for (int i = 1; i < args->container_argc; i++) {
        strcat(info.command, " ");
        strcat(info.command, args->container_argv[i]);
    }
    timestamp_to_string(created_timestamp, info.created, 20);
    strcpy(info.status, str_status[status]);
    strcpy(info.name, args->name);
    info.volume_cnt = args->volume_cnt;
    for (int i = 0; i < args->volume_cnt; i++) {
        char *rw = "rw";
        char *ro = "ro";
        char *mode = rw;
        if (args->volumes[i].ro == 1) {
            mode = ro;
        }
        sprintf(info.volumes[i], "%s:%s:%s", args->volumes[i].host, args->volumes[i].container, mode); 
    }
    return info;
}

int write_container_info(char *container_name, struct container_info *info) {
    if (!path_exist(CONTAINER_STATUS_INFO_DIR)) {
        make_path(CONTAINER_STATUS_INFO_DIR);
    }
    
    char status_file_path[1024] = {0};
    sprintf(status_file_path, "%s/%s", CONTAINER_STATUS_INFO_DIR, container_name);
    int fd = open(status_file_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        log_error("failed to open file: %s", status_file_path);
        return 1;
    }

    // 使用额外128的缓冲记录结构信息
    int info_max_size = 4096;
    char *content = (char *) malloc(sizeof(char) * info_max_size);
    memset(content, 0, info_max_size);
    sprintf(content, "pid=%d\ndetach=%d\ncontainer_id=%s\nimage=%s\ncommand=%s\ncreated=%s\nstatus=%s\nname=%s\nvolume_cnt=%d", \
    info->pid, info->detach, info->container_id, info->image, info->command, info->created, info->status, info->name, info->volume_cnt);
    for (int i = 0; i < info->volume_cnt; i++) {
        strcat(content, "\n");
        strcat(content, info->volumes[i]);
    } 
    
    if (write(fd, content, strlen(content)) == -1) {
        perror("Failed to write to file");
        close(fd);
        return 1;
    }
    close(fd);
    log_info("write container info \n%s\n in to %s", content, status_file_path);
    return 0;
}

int read_container_info(const char *container_name, struct container_info *info) {
    char container_info_file[1024] = {0};
    sprintf(container_info_file, "%s/%s", CONTAINER_STATUS_INFO_DIR, container_name);

    FILE *file = fopen(container_info_file, "r");
    if (file == NULL) {
        log_error("failed to open container info file: %s", container_info_file);
        return -1;
    }
    
    info->volume_cnt = -1;
    int vol_idx = 0;

    char line[1024];
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character from the end of the line
        line[strcspn(line, "\n")] = '\0';

        if (info->volume_cnt != -1) {
            strcpy(info->volumes[vol_idx++], line);
            continue;
        }
        // Split the line into key and value
        char *key = strtok(line, "=");
        char *value = strtok(NULL, "=");

        if (strcmp(key, "pid") == 0) {
            info->pid = atoi(value);
        }
        if (strcmp(key, "detach") == 0) {
            info->detach = atoi(value);
        }
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
        if (strcmp(key, "volume_cnt") == 0) {
            info->volume_cnt = atoi(value);
        }
    }
    fclose(file);

    if (info->volume_cnt == -1) {
        info->volume_cnt = 0; //从头到位没有读到卷挂载信息, 恢复为0;
    }
    return 0;
}


int update_container_info(char *container_name, enum container_status status) {
    struct container_info info;
    read_container_info(container_name, &info);
    char *str_status[] = {"RUNNING", "STOPPED", "EXITED"};
    strcpy(info.status, str_status[status]);
    return write_container_info(container_name, &info);
}

int list_containers_info(struct container_info *container_info_list) {
    DIR *dir = opendir(CONTAINER_STATUS_INFO_DIR);
    if (dir == NULL) {
        log_error("failed to open directory: %s", CONTAINER_STATUS_INFO_DIR);
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
        snprintf(file_path, sizeof(file_path), "%s/%s", CONTAINER_STATUS_INFO_DIR, entry->d_name);
        //printf("%s\n", file_path);
        // Check if the entry is a file
        struct stat file_stat;
        if (stat(file_path, &file_stat) == 0 && S_ISREG(file_stat.st_mode)) {
            log_info("load container info from: %s", file_path);

            struct container_info info; // 文件名对应的就是一个容器名
            int ret = read_container_info(entry->d_name, &info);
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


int remove_status_info(char *container_name) {
    char status_path[1024] = {0};
    sprintf(status_path, "%s/container_info/%s", TINYDOCKER_RUNTIME_DIR, container_name);
    return remove(status_path);
}
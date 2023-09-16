#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../logger/log.h"
#include "../util/utils.h"
#include "cgroup.h"
#include "container.h"
#include "volumes.h"
#include "workspace.h"
#include <limits.h>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>

extern char **environ;

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


int update_container_info(char *container_id, enum container_status status) {
    char container_info_dir[512] = {0};
    sprintf(container_info_dir, "%s/container_info", TINYDOCKER_RUNTIME_DIR);
    if (!path_exist(container_info_dir)) {
        make_path(container_info_dir);
    }
    
    strcat(strcat(container_info_dir, "/"), container_id);
    struct container_info info;
    read_container_info(container_info_dir, &info);


    int fd = open(container_info_dir, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        log_error("failed to open file: %s", container_info_dir);
        return 1;
    }

    char *str_status[] = {"RUNNING", "STOPPED", "EXITED"};
    // 使用额外128的缓冲记录结构信息
    ssize_t size = 128 + strlen(info.container_id) + strlen(info.image) + strlen(info.command) + strlen(info.created) + strlen(info.name);
    char *content = (char *) malloc(sizeof(char) * size);
    sprintf(content, "container_id=%s\nimage=%s\ncommand=%s\ncreated=%s\nstatus=%s\nname=%s\n", info.container_id, info.image, info.command, info.created, str_status[status], info.name);
    
    if (write(fd, content, strlen(content)) == -1) {
        perror("Failed to write to file");
        close(fd);
        return 1;
    }
    close(fd);
    log_info("update container info %s in to %s", content, container_info_dir);
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


static char child_stack[8 * 1024 * 1024];
int pipe_fd[2];

int child_fn(void *args) {
    struct docker_run_arguments *run_args = (struct docker_run_arguments *) args;
    log_info("start init inner process");
    if (init_and_set_new_root(run_args->mountpoint) != 0) {
        log_error("init docker error");
        exit(-1);
    }

    //这里阻塞等收到父进程的命令后才开始运行, 为的是等待父进程设置cgroup
    close(pipe_fd[1]);
    char input_buf[1024] = {0};
    int len = read(pipe_fd[0], input_buf, 1024);
    close(pipe_fd[0]);

    //开始运行用户命令
    char **cmds = (char **) run_args->container_argv;
    log_info("start to run %s", cmds[0]);
    int ret = execv(cmds[0], cmds);
    if (ret != 0)
        perror("exec error");
    return 0;
}

int docker_run(struct docker_run_arguments *args) {
    char *mountpoint = (char *) malloc(128 * sizeof(char));
    if (init_container_workerspace(args, mountpoint) == -1) {
        log_error("failed to init_container_workerspace");
        return -1;
    }
    log_info("create overlay filesystem mountpoint: %s", mountpoint);
    args->mountpoint = mountpoint;

    if (mount_volumes(mountpoint, args->volume_cnt, args->volumes) == -1) {
        return -1;
    }


    char cgroup_path[256] = {0};
    int ret = init_cgroup(args->name, cgroup_path); //创建一个test容器
    if (ret != 0) {
        perror("failed to init cgroup");
        return ret;
    }
    log_info("using cgroup %s", cgroup_path);

    // 注意这里cpu和mem如果设置的太小, 容器可能起不来, cpu最小要求1000, 也就是1%, mem测试能起来的最小值是204800
    if (set_cgroup_limits(cgroup_path, args->cpu, args->memory, NULL) != 0) {
        log_error("failed to set_cgroup_limits in %s", cgroup_path);
        return -1; 
    }
    log_info("set_cgroup_limits cpu=%d, mem=%d", args->cpu, args->memory);


    if (pipe(pipe_fd) == -1)
        return -1;

    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD, args);
    if (child_pid == -1) {
        perror("clone subprocess error");
    }
    log_info("docker process pid=%d", child_pid);


    int now = time(NULL);
    char container_id[64] = {0};
    sprintf(container_id, "%d", now);
    write_container_info(container_id, args->image, args->container_argv[0], now, CONTAINER_RUNNING, args->name);

    //应用cgroup限制
    if (apply_cgroup_limit_to_pid(cgroup_path, child_pid) != 0) {
        log_error("failed apply_cgroup_limit_to_pid");
        return -1;
    }

    //发送任意消息解除子进程的阻塞
    close(pipe_fd[0]);  //这里的关闭一定要放在创建子进程后面, 如果放在创建子进程前面, 由于继承关系,直接给子进程的读关闭了
    char cmds[] = "start";
    if (write(pipe_fd[1], cmds, strlen(cmds)) < 0) {
        perror("write");
    }
    close(pipe_fd[1]);


    if (waitpid(child_pid, NULL, 0) == -1) {
        perror("waitpid");
    }

    log_info("container process exit");
    update_container_info(container_id, CONTAINER_EXITED);

    //删除cgroup文件
    if (rmdir(cgroup_path) != 0) {
        log_error("failed remove cgroup: %s", cgroup_path);
        perror("failed remove cgroup");
    }
    log_info("finish clean cgroup %s", cgroup_path);

    //umount 挂载的卷
    umount_volumes(mountpoint, args->volume_cnt, args->volumes);
    log_info("finish unmount mounted volumes");

    //umount 容器跟目录挂载点
    umount(mountpoint);
    log_info("finish unmount container mountpoint %s", mountpoint);

    //清理容器工作目录
    char container_dir[256] = {0};
    sprintf(container_dir, "%s/containers/%s", TINYDOCKER_RUNTIME_DIR, args->name);
    if (remove_dir(container_dir) == -1) {
        log_error("clean container %s work dir error", container_dir);
    }
    log_info("finish clean container dir: %s", container_dir);
    
    return 0;
}



int docker_commit(struct docker_commit_arguments *args) {
    // 检查容器是否存在
    char container_mountpoint[512] = {0};
    sprintf(container_mountpoint, "%s/containers/%s/mountpoint", TINYDOCKER_RUNTIME_DIR, args->container_name);
    if (path_exist(container_mountpoint) == 0) {
        log_error("container is not exists: %s", args->container_name);
        return -1;
    }
    
    char tar_path[512] ={0};
    if (args->tar_path == NULL) {
        sprintf(tar_path, "%s.tar", args->container_name);
        args->tar_path = tar_path;
    }

    if (create_tar(container_mountpoint, args->tar_path) != 0) {
        log_error("failed to commit container: %s", args->container_name);
        return -1;
    }

    return 0;
}


int docker_ps(struct docker_ps_arguments *args) {
    struct container_info info_list[128];
    int cnt = list_containers_info(info_list);
    char *titles[] = {"CONTAINER ID", "IMAGE", "COMMAND", "CREATED", "STATUS", "NAMES", NULL};
    int spans[6] =   {12,              5,       7,         7,         6,        5}; //记录每个字段输出的最大宽度, 与titles一一对应
    for (int i = 0; i < cnt; i++) {
        if (args->list_all == 0 && strcmp(info_list[i].status, "RUNNING") != 0) {
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
        if (args->list_all == 0 && strcmp(info_list[i].status, "RUNNING") != 0) {
            continue;
        }
        printf("%-*s\t%-*s\t%-*s\t%-*s\t%-*s\t%-*s\n", spans[0], info_list[i].container_id, spans[1], info_list[i].image, \
         spans[2], info_list[i].command, spans[3], info_list[i].created, spans[4], info_list[i].status, spans[5], info_list[i].name);
    }
}
#ifndef CONTAINER_H
#define CONTAINER_H

#include "../cmdparser/cmdparser.h"


#define TINYDOCKER_RUNTIME_DIR "/home/xanarry/tinydocker_runtime"

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

int init();


int docker_run(struct docker_run_arguments *args);

int docker_commit(struct docker_commit_arguments *args);

int docker_ps(struct docker_ps_arguments *args);

#endif
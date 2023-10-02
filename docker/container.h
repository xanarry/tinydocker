#ifndef CONTAINER_H
#define CONTAINER_H

#include "../cmdparser/cmdparser.h"


#define TINYDOCKER_RUNTIME_DIR "/home/xanarry/tinydocker_runtime"
#define CONTAINER_STATUS_INFO_DIR "/home/xanarry/tinydocker_runtime/container_info"
#define CONTAINER_LOG_DIR "/home/xanarry/tinydocker_runtime/logs"

int init_docker_env();

int docker_run(struct docker_run_arguments *args);

int docker_commit(struct docker_commit_arguments *args);

int docker_ps(struct docker_ps_arguments *args);

int docker_top(struct docker_top_arguments *args);

int docker_exec(struct docker_exec_arguments *args);

int docker_stop(struct docker_stop_arguments *args);

int docker_rm(struct docker_rm_arguments *args);

#endif
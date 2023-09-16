#ifndef CONTAINER_H
#define CONTAINER_H

#include "../cmdparser/cmdparser.h"


#define TINYDOCKER_RUNTIME_DIR "/home/xanarry/tinydocker_runtime"


int init();


int docker_run(struct docker_run_arguments *args);

int docker_commit(struct docker_commit_arguments *args);

#endif
#ifndef container_h
#define container_h

#include "../cmdparser/cmdparser.h"

int init();
int docker_run(struct docker_run_arguments *args);

#endif
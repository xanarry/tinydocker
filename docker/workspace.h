#ifndef WORKERSPACE_H
#define WORKERSPACE_H

#include "../cmdparser/cmdparser.h"


//设置容器新的根目录
int init_and_set_new_root(char *new_root);

//创建只读层, 即镜像目录
int create_readonly_layer(char *image, char *readonly_dir);

//创建读写层, 即overlayfs的upperdir
int create_readwrite_layer(char *container_name, char *readwrite_dir);

//创建挂载点, 即容器实际的工作目录
int create_workespace_mount_point(char *container_name, char *lowerdir, char *upperdir, char *mountpoint);

// 初始化容器工作目录
int init_container_workerspace(struct docker_run_arguments *args, char *mountpoint);


#endif
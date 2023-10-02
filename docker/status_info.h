#ifndef STATUS_INFO_H
#define STATUS_INFO_H

enum container_status {
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_EXITED
};


struct container_info {
    int pid; //容器进程ID;
    int detach; //是否后台运行
    char container_id[128]; //容器ID
    char image[128]; //容器镜像
    char command[512]; //容器运行的命令
    char created[20]; //容器创建时间
    char status[10]; //容器状态
    char name[128]; //容器名字
    char ip_addr[16]; //容器分配的IP, 用于回收地址
    int volume_cnt; //卷挂载数量
    char volumes[32][512]; //卷挂载信息, 假定最多挂载32个卷, 卷配置host:container:ro长度不超过512
};


struct container_info create_container_info(struct docker_run_arguments *args, int pid, enum container_status status, char *ip_addr, int created_timestamp);
int write_container_info(char *container_name, struct container_info *info);
int read_container_info(const char *container_name, struct container_info *info);
int update_container_status(char *container_name, enum container_status status);
int list_containers_info(struct container_info *container_info_list);
int remove_status_info(char *container_name);
#endif
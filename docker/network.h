#ifndef NETWORK_H
#define NETWORK_H

#define TINYDOCKER_DEFAULT_NETWORK "tinydocker-0"
#define TINYDOCKER_DEFAULT_NETWORK_CIDR "172.11.11.1/24"
#define CONTAINER_NETWORKS_FILE "/home/xanarry/tinydocker_runtime/networks"

#include "../cmdparser/cmdparser.h"

struct network {
    char *name; //网络名
    char *cidr; //网段地址
    char *driver; //网络驱动名
    unsigned *used_ips; //已经分配的IP
    int used_ip_cnt; //使用的IP数量
};


int delte_network(char *name);
int create_default_bridge();
int create_network(char *name, char *cidr, char *driver);
unsigned alloc_new_ip(char *name, char *ip, int buf_size);
void list_network();
void remove_docker_network(struct docker_network_rm *cmd);
int connect_container(char *container_name, char *network, char *ip_addr);
int disconnect_container(char *container_name, char *network);


#endif
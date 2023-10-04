#ifndef NETWORK_H
#define NETWORK_H

#define TINYDOCKER_DEFAULT_NETWORK_NAME "tinydocker-0"
#define TINYDOCKER_DEFAULT_NETWORK_CIDR "172.11.11.0/24"
#define TINYDOCKER_DEFAULT_IP_ADDR_CIDR "172.11.11.1/24"
#define TINYDOCKER_DEFAULT_GATEWAY "172.11.11.1"
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
int release_used_ip(char *name, char *ip);
void list_network();
void remove_docker_network(struct docker_network_rm *cmd);
int connect_container(char *container_name, char *network, char *ip_addr);
int disconnect_container(char *container_name, char *network);
void set_container_port_map(char *container_ip, int port_cnt, struct port_map *port_maps);
void unset_container_port_map(char *container_ip);
void get_first_cidr_host_ip(char *cidr_network, char *cidr_host_ip, int size);

#endif
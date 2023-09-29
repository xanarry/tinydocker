#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "network.h"
#include "cgroup.h"
#include "../logger/log.h"


#define NETWORK_CONFIG_FILE "./network"


unsigned int str_ip_to_int(char *ip) {
    struct in_addr addr;
    addr.s_addr = inet_addr(ip);
    inet_aton(ip, &addr);
    return addr.s_addr;
}

void int_to_str_ip(unsigned int int_ip, char *ip_buf, int ip_buf_size) {
    struct in_addr addr;
    addr.s_addr = int_ip;
    inet_ntop(AF_INET, &addr, ip_buf, ip_buf_size);
}


void print_network(struct network *nw) {
    printf("   name: %s\n", nw->name);
    printf(" driver: %s\n", nw->driver);
    printf("   cidr: %s\n", nw->cidr);
    printf("used_ip: ");
    for (int i = 0; i < nw->used_ip_cnt; i++) {
        char str_ip_buf[64] = {0};
        int_to_str_ip(nw->used_ips[i], str_ip_buf, 64);
        printf("%s%u ", str_ip_buf, nw->used_ips[i]);
    }
    puts("\n");
}

int write_network_info(struct network nw) {
    FILE* file = fopen(NETWORK_CONFIG_FILE, "a");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }

    //name:driver:cidr:ip1;ip2;ip3
    char buf[4096] = {0};
    sprintf(buf, "%s:%s:%s:", nw.name, nw.driver, nw.cidr);
    
    puts(buf);
    for (int i = 0; i < nw.used_ip_cnt; i++) {
        char int_str[32] = {0};
        sprintf(int_str, "%u;", nw.used_ips[i]);
        strcat(buf, int_str);
    }
    log_info("write network info: %s", buf);
    int ret = fprintf(file, "%s\n", buf);
    fclose(file);
    return ret > 0 ? 0 : -1;
}  


int get_network_list(struct network *nw_buffer, int bufsize) {
    FILE* file = fopen(NETWORK_CONFIG_FILE, "r");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }

    //name:driver:cidr:ip1;ip2;ip3
    int line_cnt = 0;
    char line[4096] = {0};
    while (fgets(line, sizeof(line), file) != NULL) {
        struct network nw;
        nw.name = (char *) malloc(64);
        nw.driver = (char *) malloc(64);
        nw.cidr = (char *) malloc(64);
        nw.used_ips = (unsigned *) malloc(sizeof(unsigned) * 128);

        line[strcspn(line, "\n")] = '\0';
        char* token = strtok(line, ":");
        strcpy(nw.name, token);
        token = strtok(NULL, ":");
        strcpy(nw.driver, token);
        token = strtok(NULL, ":");
        strcpy(nw.cidr, token);


        char *ip_list = strtok(NULL, ":");

        printf("read %s |: %s %s %s %s\n", line, nw.name, nw.driver, nw.cidr, ip_list);
        char *ip_token = strtok(ip_list, ";");
        int ip_cnt = 0;
        while (ip_token != NULL) {
            nw.used_ips[ip_cnt++] = (unsigned) atol(ip_token);
            ip_token = strtok(NULL, ";");
        }
        nw.used_ip_cnt = ip_cnt;
        nw_buffer[line_cnt++] = nw;
        if (line_cnt == bufsize) {
            break;
        }
        memset(line, 0, 4096);
    }
    fclose(file);

    printf("list end, get %d lines\n", line_cnt);
    return line_cnt;
}


int read_network_info(char *name, struct network *nw) {
    int bufsize = 128;
    struct network *nw_buffer = malloc(sizeof(struct network) * bufsize);
    int size = get_network_list(nw_buffer, bufsize);
    int ok = -1;
    for (int i = 0; i < size; i++) {
        puts("pppppppppppppp");
        if (strcmp(nw_buffer[i].name, name) == 0) {
            memcpy(nw, &nw_buffer[i], sizeof(struct network));
            ok = 0;
            break;
        }
    }
    return ok;
}


int delete_network_info(char *name) {
    int bufsize = 128;
    struct network *nw_buffer = malloc(sizeof(struct network) * bufsize);
    int size = get_network_list(nw_buffer, bufsize);

    // 打开文件以清空内容
    FILE *file = fopen(NETWORK_CONFIG_FILE, "w");
    if (file == NULL) {
        log_error("failed update network info, can not open %s", NETWORK_CONFIG_FILE);
        return -1;
    }
    fclose(file);

    // 重新写入数据到文件
    for (int i = 0; i < size; i++) {
        if (strcmp(nw_buffer[i].name, name) == 0) {
            continue;
        }
        if (write_network_info(nw_buffer[i]) == 0) {
            log_error("failed update network info");
            return -1;
        }
    }
    return 0;
}


int net_has_exist(char *brname) {
    char check_cmd[128] = {0};
    // 检查网桥是不是已经存在
    sprintf(check_cmd, "brctl show | grep -q '^%s'", brname);
    if (system(check_cmd) != 0) {
        return 0;
    }
    return 1;
}

int create_network(char *name, char *cidr, char *driver) {
    // 暂时只支持网桥
    if (strcmp(driver, "bridge") == 0) {
        if (net_has_exist(name)) {
            log_error("network %s has exists");
            return -1;
        }

        char bradd_cmd[128] = {0};
        sprintf(bradd_cmd, "if ! brctl show | grep -q \"^%s\"; then brctl addbr \"%s\"; fi", name, name);

        char set_ip_cmd[128] = {0};
        sprintf(set_ip_cmd, "ip addr add %s dev %s", cidr, name);

        char start_up[128] = {0};
        sprintf(start_up, "ip link set %s up", name);

        char *cmds[] = {bradd_cmd, set_ip_cmd, start_up};
        for (int i = 0; i < 3; i++) {
            system(cmds[i]);
        }
    }

    struct network nw = {
        .name = name,
        .driver = driver,
        .cidr = cidr,
        .used_ips = NULL,
        .used_ip_cnt = 0
    };

    return write_network_info(nw);
}


int update_network_info(char *name, struct network *nw) {
    int bufsize = 128;
    struct network *nw_buffer = malloc(sizeof(struct network) * bufsize);
    int size = get_network_list(nw_buffer, bufsize);

     // 打开文件以清空内容
    FILE *file = fopen(NETWORK_CONFIG_FILE, "w");
    if (file == NULL) {
        log_error("failed update network info, can not open %s", NETWORK_CONFIG_FILE);
        return -1;
    }
    fclose(file);

    // 重新写入数据到文件
    for (int i = 0; i < size; i++) {
        print_network(&nw_buffer[i]);
        if (strcmp(nw_buffer[i].name, nw->name) == 0) {
            nw_buffer[i] = *nw; //用新的覆盖旧的
        }
        print_network(nw);
        if (write_network_info(nw_buffer[i]) == -1) {
            log_error("failed update network info");
            return -1;
        }
    }
    return 0;
}

int create_default_bridge() {
    if (net_has_exist(TINYDOCKER_DEFAULT_NETWORK)) {
        return 0;
    }
    return create_network(TINYDOCKER_DEFAULT_NETWORK, TINYDOCKER_DEFAULT_NETWORK_CIDR, "bridge");
}


int delte_network(char *name) {
    char close_if[128] = {0};
    sprintf(close_if, "ip link set dev %s down", name);
    system(close_if);

    char delte_br[128] = {0};
    sprintf(delte_br, "sudo brctl delbr %s", name);
    system(delte_br);

    delete_network_info(name);
    return 0;
}



void get_CIDR_range(const char* cidr, unsigned int *minIP, unsigned int *maxIP) {
    char network[16];
    int prefix;
    sscanf(cidr, "%[^/]/%d", network, &prefix);

    unsigned int ip = ntohl(str_ip_to_int(network));
    unsigned int mask = 0xFFFFFFFF << (32 - prefix);

    *minIP = htonl(ip & mask);
    *maxIP = htonl(ip | (~mask));
}

unsigned alloc_new_ip(struct network *nw) {
    unsigned int minIP;
    unsigned int maxIP;
    get_CIDR_range(nw->cidr, &minIP, &maxIP);
    //分配IP地址ID时候排除最小的IP和最大的IP和已经被分配的IP
    unsigned int_ip = 0;
    for (unsigned i = minIP + 1; i < maxIP - 1; i++) {
        int used = 0;
        for (int j = 0; j < nw->used_ip_cnt; j++) {
            if (i == nw->used_ips[j]) {
                used = 1;
            }
        }
        if (used == 0) {
            int_ip = i;
            break;
        }
    }

    nw->used_ips[nw->used_ip_cnt++] = int_ip;
    if (update_network_info(nw->name, nw) == -1) {
        log_info("failed to update network info: %s", nw->name);
    }
    
    return 0;
}

void list_network() {
    struct network nw_buffer[100];
    int cnt = get_network_list(nw_buffer, 100);
    printf("%-10s\t%s\t%-18s\t%s\n", "NAME", "DRIVER", "CIDR", "ALLOC_IPS");
    for (int i = 0; i < cnt; i++) {
        printf("%-10s\t%s\t%-18s\t", nw_buffer[i].name, nw_buffer[i].driver, nw_buffer[i].cidr);
        char ips[4096] = {0};
        for (int j = 0; j < nw_buffer[i].used_ip_cnt; j++) {
            char str_ips[64] = {0};
            int_to_str_ip(nw_buffer[i].used_ips[j], str_ips, 64);
            strcat(ips, str_ips);
            strcat(ips, ",");
        }
        if (strlen(ips) == 0) {
            strcpy(ips, "NULL");
        }
        printf("%s\n", ips);
    }
}

void remove_docker_network(struct docker_network_rm *cmd) {
    for (int i = 0; i < cmd->network_argc; i++) {
        delte_network(cmd->network_argv[i]);
    }
}


int connect_container(char *container_name, char *network) {
    char *veth_pair[2] = {"vth0", "bridge-vth"};
    char cmd[1024] = {0};
    sprintf(cmd, "ip link add %s-%s type veth name %s-%s", container_name, veth_pair[0], container_name, veth_pair[1]);
    if (system(cmd) != 0) {
        log_error("failed to create veth pair for container: %s", container_name);
        return -1;
    }

    sprintf(cmd, "brctl addif %s %s-%s", network, container_name, veth_pair[1]);
    if (system(cmd) != 0) {
        log_error("failed to addif %s-%s to bridge %s", container_name, veth_pair[1], network);
        return -1;
    }
    

    // 找出目标容器中的一个进程ID, 该进程用来寻找ns文件
    int pid_list[4096];
    int pid_cnt = get_container_processes_id(container_name, pid_list);
    if (pid_cnt <= 0) {
        log_error("failed to get container process list");
        return -1;
    }
    //默认第一个进程为1号进程, 可能不准, 但是在我们这个简单环境下基本都是它了
    int one_pid = pid_list[0];

    sprintf(cmd, "ip link set dev %s-%s netns %d", container_name, veth_pair[0], one_pid);
    if (system(cmd) != 0) {
        log_error("failed to addif %s-%s to container", container_name, veth_pair[0], network);
        return -1;
    }
    

    //为容器里面的端口设置IP并启动
    struct network nw;
    read_network_info(network, &nw);
    unsigned ip = alloc_new_ip(&nw);
    char str_ip[64] = {0};
    int_to_str_ip(ip, str_ip, 64);
    sprintf(cmd, "nsenter -t %d -n ip addr add %s/24 dev %s-%s", one_pid, str_ip, container_name, veth_pair[0]);
    if (system(cmd) != 0) {
        log_error("failed to set ip %s for %s-%s", str_ip, container_name, veth_pair[0]);
        return -1;
    }

    sprintf(cmd, "nsenter -t %d -n ip link set %s-%s up", one_pid, container_name, veth_pair[0]);
    if (system(cmd) != 0) {
        log_error("failed to set ip %s for %s-%s", str_ip, container_name, veth_pair[0]);
        return -1;
    }
    return 0;
}
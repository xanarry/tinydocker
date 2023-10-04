#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include "network.h"
#include "cgroup.h"
#include "status_info.h"
#include "../cmdparser/cmdparser.h"
#include "../logger/log.h"


//IP地址转主机序整数
unsigned int str_ip_to_int(char *ip) {
    struct in_addr addr;
    addr.s_addr = inet_addr(ip);
    inet_aton(ip, &addr);
    return ntohl(addr.s_addr);
}

//主机序列整数转字符串IP
void int_to_str_ip(unsigned int int_ip, char *ip_buf, int ip_buf_size) {
    struct in_addr addr;
    addr.s_addr = htonl(int_ip);
    inet_ntop(AF_INET, &addr, ip_buf, ip_buf_size);
}

//获取IP地址的有效范围
void get_CIDR_range(const char* cidr, unsigned int *minIP, unsigned int *maxIP) {
    char network[16];
    int prefix;
    sscanf(cidr, "%[^/]/%d", network, &prefix);

    unsigned int ip = str_ip_to_int(network);
    unsigned int mask = 0xFFFFFFFF << (32 - prefix);

    *minIP = ip & mask; //htonl
    *maxIP = ip | (~mask);
}


void print_network(struct network *nw) {
    printf("   name: %s\n", nw->name);
    printf(" driver: %s\n", nw->driver);
    printf("   cidr: %s\n", nw->cidr);
    printf("used_ip: ");
    for (int i = 0; i < nw->used_ip_cnt; i++) {
        char str_ip_buf[64] = {0};
        int_to_str_ip(nw->used_ips[i], str_ip_buf, 64);
        printf("%s|%u ", str_ip_buf, nw->used_ips[i]);
    }
    puts("\n");
}

int write_network_info(struct network nw) {
    FILE* file = fopen(CONTAINER_NETWORKS_FILE, "a");
    if (file == NULL) {
        log_error("failed to open %s, error: %s", CONTAINER_NETWORKS_FILE, strerror(errno));
        exit(1);
    }

    //name:driver:cidr:ip1;ip2;ip3
    char buf[4096] = {0};
    sprintf(buf, "%s:%s:%s:", nw.name, nw.driver, nw.cidr);
    for (int i = 0; i < nw.used_ip_cnt; i++) {
        char int_str[32] = {0};
        sprintf(int_str, "%u;", nw.used_ips[i]);
        strcat(buf, int_str);
    }
    //log_info("write network info: %s", buf);
    int ret = fprintf(file, "%s\n", buf);
    fclose(file);
    return ret > 0 ? 0 : -1;
}  


int get_network_list(struct network *nw_buffer, int bufsize) {
    FILE* file = fopen(CONTAINER_NETWORKS_FILE, "r");
    if (file == NULL) {
        log_error("failed to open %s, error: %s", CONTAINER_NETWORKS_FILE, strerror(errno));
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

        //printf("read %s |: %s %s %s %s\n", line, nw.name, nw.driver, nw.cidr, ip_list);
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

    //printf("list end, get %d lines\n", line_cnt);
    return line_cnt;
}


int read_network_info(char *name, struct network *nw) {
    int bufsize = 128;
    struct network *nw_buffer = malloc(sizeof(struct network) * bufsize);
    int size = get_network_list(nw_buffer, bufsize);
    int ok = -1;
    for (int i = 0; i < size; i++) {
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
    FILE *file = fopen(CONTAINER_NETWORKS_FILE, "w");
    if (file == NULL) {
        log_error("failed update network info, can not open %s", CONTAINER_NETWORKS_FILE);
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
    if (strcmp(driver, "bridge") != 0) {
        return -1;
    }

    //如果已经存在该网络
    if (net_has_exist(name)) {
        log_error("network %s has exists");
        return -1;
    }

    struct network nw = {
        .name = name,
        .driver = driver,
        .cidr = cidr,
        .used_ips = NULL,
        .used_ip_cnt = 0
    };

    //创建网桥
    char bradd_cmd[128] = {0};
    sprintf(bradd_cmd, "if ! brctl show | grep -q \"^%s\"; then brctl addbr \"%s\"; fi", name, name);
    if (system(bradd_cmd) != 0) {
        log_error("failed create new bridge for %s", name);
        return -1;
    }

    //写入网络信息
    if (write_network_info(nw) == -1) {
        log_error("failed to save network info: %s", name);
        return -1;
    }

    //为网桥设置IP地址
    char set_ip_cmd[128] = {0};
    sprintf(set_ip_cmd, "ip addr add %s dev %s", TINYDOCKER_DEFAULT_IP_ADDR_CIDR, name); //这里使用cidr地址

    char start_up[128] = {0};
    sprintf(start_up, "ip link set %s up", name);

    char *cmds[] = {set_ip_cmd, start_up, NULL};
    for (int i = 0; cmds[i] != NULL; i++) {
        if(system(cmds[i]) != 0) {
            log_warn("faild to run %s", cmds[i]);
        }
    }
    return 0;
}


int update_network_info(char *name, struct network *nw) {
    int bufsize = 128;
    struct network *nw_buffer = malloc(sizeof(struct network) * bufsize);
    int size = get_network_list(nw_buffer, bufsize);

     // 打开文件以清空内容
    FILE *file = fopen(CONTAINER_NETWORKS_FILE, "w");
    if (file == NULL) {
        log_error("failed update network info, can not open %s", CONTAINER_NETWORKS_FILE);
        return -1;
    }
    fclose(file);

    // 重新写入数据到文件
    for (int i = 0; i < size; i++) {
        if (strcmp(nw_buffer[i].name, nw->name) == 0) {
            nw_buffer[i] = *nw; //用新的覆盖旧的
        }
        if (write_network_info(nw_buffer[i]) == -1) {
            log_error("failed update network info");
            return -1;
        }
    }
    return 0;
}

int create_default_bridge() {
    //如果网桥不存在就创建
    int ret_val = 0;
    if (net_has_exist(TINYDOCKER_DEFAULT_NETWORK_NAME) == 0) {
        log_info("init detail bridge network %s for tinydocker", TINYDOCKER_DEFAULT_NETWORK_NAME);
        ret_val = create_network(TINYDOCKER_DEFAULT_NETWORK_NAME, TINYDOCKER_DEFAULT_NETWORK_CIDR, "bridge");
    }

    //存在无论如何都启动一把
    char set_up_cmd[128] = {0};
    sprintf(set_up_cmd, "ip link set %s up", TINYDOCKER_DEFAULT_NETWORK_NAME);
    system(set_up_cmd);

    return ret_val;
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


unsigned alloc_new_ip(char *name, char *ip, int buf_size) {
    struct network nw;
    if (read_network_info(name, &nw) == -1) {
        log_error("failed to read newwork info of %s", name);
        return 0;
    }

    unsigned int minIP;
    unsigned int maxIP;
    get_CIDR_range(nw.cidr, &minIP, &maxIP);
    //分配IP地址ID时候排除最小的IP和最大的IP和已经被分配的IP
    unsigned int_ip = 0;
    for (unsigned i = minIP + 2; i < maxIP - 1; i++) { //加2开始时为了避免分配主机号0,1和255, 1是这里被设置为网桥地址
        int used = 0;
        for (int j = 0; j < nw.used_ip_cnt; j++) {
            if (i == nw.used_ips[j]) {
                used = 1;
            }
        }
        if (used == 0) {
            int_ip = i;
            break;
        }
    }

    nw.used_ips[nw.used_ip_cnt++] = int_ip;
    if (update_network_info(nw.name, &nw) == -1) {
        log_info("failed to update network info: %s", nw.name);
    }
    
    if (int_ip > 0) { //如果拿到了有效IP, 那么就做地址转换
        int_to_str_ip(int_ip, ip, buf_size);
    }

    return int_ip;
}


int release_used_ip(char *name, char *ip) {
    struct network nw;
    if (read_network_info(name, &nw) == -1) {
        log_error("failed to read network info of %s", name);
        return -1;
    }

    unsigned *old_ips = (unsigned *) malloc(sizeof(unsigned) * nw.used_ip_cnt);
    for (int i = 0; i < nw.used_ip_cnt; i++) {
        old_ips[i] = nw.used_ips[i];
    }

    unsigned int_ip = str_ip_to_int(ip);
    int new_size = 0;
    for (int i = 0; i < nw.used_ip_cnt; i++) {
        if (old_ips[i] != int_ip) {  //回写是排除当前释放的IP地址
            nw.used_ips[new_size++] = old_ips[i];
        }
    }
    nw.used_ip_cnt = new_size;

    free(old_ips);

    int ret = update_network_info(nw.name, &nw);
    if (ret == -1) {
        log_info("failed to update network info: %s", nw.name);
    }
    
    return ret;
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


int connect_container(char *container_name, char *network, char *ip_addr) {
    //为容器申请新的IP
    char str_ip[64] = {0}; //返回0表示没有申请到有效的IP
    if (alloc_new_ip(network, str_ip, 64) == 0) {
        log_error("failed alloc new ip for container: %s", container_name);
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


    //生成veth的名字
    char *veth_container = "eth0";
    char veth_host[20] = {0};
    snprintf(veth_host, 16, "%s-%s", "tdbr", container_name); //veth名字长度显示是16

    //创建veth peer
    char add_veth_peer[128] = {0};
    sprintf(add_veth_peer, "ip link add %s type veth peer name %s", veth_container, veth_host);

    //将veth一端连接到网桥
    char brctl_addif[128] = {0};
    sprintf(brctl_addif, "brctl addif %s %s", network, veth_host);
    //并启动网桥端的veth
    char set_host_peer_up[128] = {0};
    sprintf(set_host_peer_up, "ip link set %s up", veth_host);

    //将veth的另一端放入容器中
    char set_veth_container_into_container[128] = {0};
    sprintf(set_veth_container_into_container, "ip link set dev %s netns %d", veth_container, one_pid);

    //设置容器回环地址
    char add_loif[128];
    sprintf(add_loif, "nsenter -t %d -n ip addr add 127.0.0.1/8 dev lo", one_pid);
    char set_loifup[128];
    sprintf(set_loifup, "nsenter -t %d -n ip link set lo up", one_pid);


    //设置容器网桥地址
    char add_container_addr[128];
    sprintf(add_container_addr, "nsenter -t %d -n ip addr add %s/24 dev %s", one_pid, str_ip, veth_container);
    char set_container_ifup[128];
    sprintf(set_container_ifup, "nsenter -t %d -n ip link set %s up", one_pid, veth_container);


    //设置容器路由
    char set_route[128] = {0};
    sprintf(set_route, "nsenter -t %d -n ip route add default via %s dev eth0", one_pid, TINYDOCKER_DEFAULT_GATEWAY);
        
    //开始执行上述命令
    char *cmds[] = {add_veth_peer, brctl_addif, set_host_peer_up, set_veth_container_into_container, \
                    add_loif, set_loifup, add_container_addr, set_container_ifup, set_route, NULL};

    for (int i = 0; cmds[i] != NULL; i++) {
        if (system(cmds[i]) != 0) {
            log_error("failed run: %s", cmds[i]);
            return -1;
        }
    }

    strcpy(ip_addr, str_ip);
    return 0;
}

int disconnect_container(char *container_name, char *network) {
    char veth_host[20] = {0};
    snprintf(veth_host, 16, "%s-%s", "tdbr", container_name); //veth名字长度显示是16

    //关闭host段的peer, 然后从默认网桥下拔出
    char set_host_peer_down[128] = {0};
    sprintf(set_host_peer_down, "ip link set %s down", veth_host);
    
    char brctl_delif[128] = {0};
    sprintf(brctl_delif, "brctl delif %s %s", network, veth_host);

    //开始执行上述命令
    char *cmds[] = {brctl_delif, set_host_peer_down, NULL};
    for (int i = 0; cmds[i] != NULL; i++) {
        if (system(cmds[i]) != 0) {
            log_error("failed run: %s", cmds[i]);
            return -1;
        }
    }
    return 0;
}


void set_container_port_map(char *container_ip, int port_cnt, struct port_map *port_maps) {
    for (int i = 0; i < port_cnt; i++) {
        int host_port = port_maps[i].host_port;
        int container_port = port_maps[i].container_port;

        char output_dnat[128] = {0}; //用户本机访问容器的端口转发, 因为PREROUTING只对来自宿主机外部请求起作用
        sprintf(output_dnat, "iptables -t nat -A OUTPUT -p tcp --dport %d -j DNAT --to-destination %s:%d", host_port, container_ip, container_port);
        char prerouting_dnat[128] = {0}; //用户外部机器访问host主机时的端口转发
        sprintf(prerouting_dnat, "iptables -t nat -A PREROUTING -p tcp -m tcp --dport %d -j DNAT --to-destination %s:%d", host_port, container_ip, container_port);
       
        char *cmds[] = {output_dnat, prerouting_dnat, NULL};
        for (int i = 0; cmds[i] != NULL; i++) {
            int ret = system(cmds[i]);
            if (ret != 0) {
                log_error("failed to run %s, ret:%d", cmds[i], ret);
            }
        }
        log_info("set host port %d map to container port %d successful", host_port, container_port);
    }
}

void unset_container_port_map(char *container_ip) {
    char pattern[64] = {0};
    sprintf(pattern, "DNAT --to-destination %s", container_ip);
    char bash_script[1024] = {0};
    sprintf(bash_script, "iptables -t nat -S | grep '%s' | while IFS= read -r rule; do iptables -t nat -D ${rule#\"-A\"}; done", pattern);
    int ret = system(bash_script);
    if (ret == 0) {
        log_info("unset  container port map for container ip %s successful", container_ip);
    } else {
        log_info("unset  container port map for container ip %s ret:%d", container_ip, ret);
    }
}
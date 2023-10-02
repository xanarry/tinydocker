#define _GNU_SOURCE
#include <sys/mount.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ftw.h>
#include <openssl/evp.h>
#include <limits.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>


#define TINYDOCKER_RUNTIME_DIR "/home/xanarry/tinydocker_runtime"


int path_exist(const char *path) {
    if (access(path, F_OK) == 0)
        return 1;
    return 0;
}

int make_path(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!path_exist(tmp) && mkdir(tmp, S_IRWXU) == -1) {
                return -1;
            }
            *p = '/';
        }
    }
    return path_exist(tmp) ? 0 : mkdir(tmp, S_IRWXU);
}


struct container_info1 {
    char *container_id; //容器ID
    char *image; //容器镜像
    char *command; //容器运行的命令
    int created; //容器创建时间
    int status; //容器状态
    char *name; //容器名字
};


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

#define MAX_BUF 1024
#define MAX_CGROUPS 10
#define CGROUP_ROOT "/sys/fs/cgroup"




static char child_stack[8 * 1024 * 1024];
int pipe_fd[2];

extern char **environ;
int child_fn(void *args) {
    // //这里阻塞等收到父进程的命令后才开始运行, 为的是等待父进程设置cgroup
    // close(pipe_fd[1]);
    // char input_buf[1024] = {0};
    // int len = read(pipe_fd[0], input_buf, 1024);
    // close(pipe_fd[0]);

    printf("child start %d\n", getpid());

    char *cmds[] = {"/bin/sh", NULL};
    int ret = execv(cmds[0], cmds);
    //int ret = execv(cmds[0], cmds);
    if (ret != 0)
        perror("exec error");
    return 0;
}

int docker_run() {

    if (pipe(pipe_fd) == -1)
        return -1;
/*
    // 这里不要加CLONE_NEWUSER, 否则会导致pivot_root权限不足, 
    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWPID | CLONE_NEWNS | SIGCHLD, NULL);
    if (child_pid == -1) {
        perror("clone subprocess error");
    }
    printf("docker process pid= %d \n", child_pid);
*/
    int pid = fork();
    if (pid < 0) {
        perror("fork");
        return 0;
    } else if (pid == 0) {
        child_fn(NULL);
    } else {
        printf("docker process pid= %d \n", pid);
        // //发送任意消息解除子进程的阻塞
        // close(pipe_fd[0]);  //这里的关闭一定要放在创建子进程后面, 如果放在创建子进程前面, 由于继承关系,直接给子进程的读关闭了
        // char cmds[] = "start";
        // if (write(pipe_fd[1], cmds, strlen(cmds)) < 0) {
        //     perror("write");
        // }
        // close(pipe_fd[1]);
    }
    return 0;
}



int mainx(int argc, char const *argv[])
{
    //docker_run();
    int pid = fork();
    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        printf("child start %d\n", getpid());
        char *cmds[] = {"/bin/sh", NULL};
        int ret = execv(cmds[0], cmds);
    } else {
        printf("docker process pid= %d \n", pid);
    }
}


int mai2n(int argc, char const *argv[])
{
    int pid = fork();
    if (pid < 0) {
        perror("fork");
    } else if (pid == 0) {
        char *cmd[] = {"/bin/sh", NULL};
        execv(cmd[0], cmd);
    } else {
        printf("father wait: %d\n", pid);
        //wait(NULL);
    }
    return 0;
}


#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <stdio.h>
#include <arpa/inet.h>



#define MAX_IPS 256

int* getUsedIPs(const char* cidr, const char* iplistFile, int* count) {
    FILE* file = fopen(iplistFile, "r");
    if (file == NULL) {
        perror("fopen");
        exit(1);
    }

    char line[256];
    int lineCount = 0;
    int* usedIPs = malloc(MAX_IPS * sizeof(int));
    if (usedIPs == NULL) {
        perror("malloc");
        exit(1);
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\n")] = '\0';
        char* token = strtok(line, ":");
        if (strcmp(token, cidr) == 0) {
            token = strtok(NULL, ":");
            char* ipToken = strtok(token, ";");
            while (ipToken != NULL) {
                usedIPs[lineCount++] = atoi(ipToken);
                ipToken = strtok(NULL, ";");
            }
            break;
        }
    }

    fclose(file);

    *count = lineCount;
    return usedIPs;
}

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




int main() {
    char* ip = "177.11.11.5";
    unsigned uip = str_ip_to_int(ip);
    printf("%u, %u\n", uip, ntohl(uip));
    //char *sip = int_to_str_ip(ntohl(723889u));
    //printf("%s\n", sip);
    

    //整数ip转换为ip字符串
    printf("==========: %u\n", str_ip_to_int("192.168.0.0"));

    const char* cidr = "177.11.11.1/16";
    unsigned int minIP, maxIP;

    get_CIDR_range(cidr, &minIP, &maxIP);

    printf("CIDR: %s\n", cidr);
    //printf("Min IP: %u, %s\n", minIP, int_to_str_ip(minIP));
    //printf("Max IP: %u, %s\n", maxIP, int_to_str_ip(maxIP));
    for (unsigned i = minIP; i <= maxIP; i++) {
        char buf[100];
        int_to_str_ip(i, buf, 100);
        printf("%u -> %s\n", i, buf);
    }


    const char* iplistFile = "iplist.txt";
    int count = 0;
    int* usedIPs = getUsedIPs(cidr, iplistFile, &count);

    printf("CIDR: %s\n", cidr);
    printf("Used IPs:\n");
    for (int i = 0; i < count; i++) {
        printf("%d\n", usedIPs[i]);
    }

    free(usedIPs);
    char bradd_cmd[128] = {0};
    char *brname = "test";
    sprintf(bradd_cmd, "if ! brctl show | grep -q \"^%s\"; then brctl addbr \"%s\"; fi", brname, brname);
    puts(bradd_cmd);
    int r = system(bradd_cmd);
    return 0;
}
/*
192.168.0.0/23:123;345;657;2321;3234;9

*/
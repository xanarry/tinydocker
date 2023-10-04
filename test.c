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
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <arpa/inet.h>

int pipe_fd[2]; //无名管道
static char child_stack[8 * 1024 * 1024];

//设置容器新的根目录
int set_new_root(char *new_root) {
    /* Ensure that 'new_root' and its parent mount don't have shared propagation (which would cause pivot_root() to
       return an error), and prevent propagation of mount events to the initial mount namespace. 
       保证new_root与它的父挂载点没有共享传播属性, 否则调用pivot_root会报错
       */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        perror("mount /");  return -1;
    }

    /* 保证'new_root'是一个独立挂载点, 如果有挂载卷在new_root, 这里务必加上MS_REC参数递归挂载挂载的卷目录, 否则容器里面看不到内容*/
    if (mount(new_root, new_root, NULL, MS_REC | MS_BIND, NULL) == -1) {
       perror("mount new_root"); return -1;
    }

    char mkdir_cmd[256] = {0};
    sprintf(mkdir_cmd, "mkdir -p %s/%s", new_root, "old_root");
    system(mkdir_cmd);

    char old_root[256] = {0};
    sprintf(old_root, "%s/%s", new_root, "old_root");
    if (syscall(SYS_pivot_root, new_root, old_root) != 0) {
        perror("SYS_pivot_root"); return -1;
    }

    chdir("/");

    // 卸载老的root
    char old_new[100] = "/old_root";
    if (umount2(old_new, MNT_DETACH) != 0) {
        perror("umount2 old_root"); return -1;
    }
    
    char del_cmd[512] = {0};
    sprintf(del_cmd, "rm -rf %s", old_root);
    return system("rm -rf /old_root");
}

int child_fn(void *args) {
    char *new_root = (char *) args;
    if (set_new_root(new_root) != 0) {
        return -1;
    }

    close(pipe_fd[1]);
    char input_buf[1024] = {0};
    read(pipe_fd[0], input_buf, 1024); //阻塞等待父进程的通知
    close(pipe_fd[0]);

    execl("/bin/sh", "/bin/sh", NULL); //在子进程中运行/bin/sh
}

void get_first_host_ip(const char* cidr_network, char *cidr_host_ip) {
    int a, b, c, d, n;
    // 解析CIDR网络
    sscanf(cidr, "%u.%u.%u.%u/%u", &a, &b, &c, &d, &n);
    // 计算第一个主机IP
    sprintf(cidr_host_ip, "%u.%u.%u.%u/%u", a, b, c, 1, n);
}

int main(int argc, char *argv[]) {
    char *t = "172.11.11.1/24"
    return 0;
    pipe(pipe_fd);
    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD, argv[1]);
    if (child_pid == -1) {
        perror("clone subprocess error");
    }

    close(pipe_fd[0]);  //这里的关闭一定要放在创建子进程后面, 如果放在创建子进程前面, 由于继承关系,直接给子进程的读关闭了
    char cmds[] = "start";
    write(pipe_fd[1], cmds, strlen(cmds)); //发送任意字符串
    close(pipe_fd[1]);

    wait(NULL); // 等待子进程结束
    printf("子进程已结束\n");
    return 0;
}

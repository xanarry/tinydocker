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
#include <fcntl.h>
#include <pty.h>
#include <stdio.h>
#include <utmp.h>
#include <sys/ioctl.h>
#include <unistd.h>

int main() {
    for (int i = 0; i< 10000; i++) {
        printf("xxxxsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssxxxxxxsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssxxxxxxsssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssssxx %d\n", i);
        sleep(1);
    }
    return 0;
}
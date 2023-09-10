#define _GNU_SOURCE
#include <sys/mount.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "../cmdparser/cmdparser.h"
#include "../logger/log.h"
#include "cgroup/cgroup.h"

extern char **environ;

int init() {
    //int mount(const char *source, const char *target, const char *filesystemtype, unsigned long mountflags, const void *_Nullable data);
    int ret = mount("", "/", NULL, MS_REC|MS_PRIVATE, NULL);
    if (ret != 0) {
        perror("set / mount point as private error");
        return ret;
    }

    ret = mount("proc", "/proc", "proc", MS_NOEXEC|MS_NOSUID|MS_NODEV, NULL);
    if (ret != 0) {
        perror("mount proc error");
        return ret;
    }

    return 0;
}


static char child_stack[8 * 1024 * 1024];
int pipe_fd[2];

int child_fn(void *args) {
    log_info("start init inner process");
    if (init() != 0) {
        log_error("init docker error");
        exit(-1);
    }

    close(pipe_fd[1]);
    char input_buf[1024] = {0};
    int len = read(pipe_fd[0], input_buf, 1024);
    close(pipe_fd[0]);
    char **cmds = (char **) args;
    log_info("start to run %s", cmds[0]);
    int ret = execv(cmds[0], cmds);
    if (ret != 0)
        perror("exec error");
    return 0;
}

int docker_run(struct docker_run_arguments *args) {
    char cgroup_path[256] = {0};
    int ret = init_cgroup("test", cgroup_path); //创建一个test容器
    if (ret != 0) {
        perror("failed to init cgroup");
        return ret;
    }
    log_info("using cgroup %s", cgroup_path);

    // 注意这里cpu和mem如果设置的太小, 容器可能起不来, cpu最小要求1000, 也就是1%, mem测试能起来的最小值是204800
    if (set_cgroup_limits(cgroup_path, args->cpu, args->memory, NULL) != 0) {
        log_error("failed to set_cgroup_limits in %s", cgroup_path);
        return -1; 
    }
    log_info("set_cgroup_limits cpu=%d, mem=%d", args->cpu, args->memory);


    if (pipe(pipe_fd) == -1)
        return -1;

    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD, args->container_argv);
    if (child_pid == -1) {
        perror("clone subprocess error");
    }
    log_info("docker process pid=%d", child_pid);


    //应用cgroup限制
    if (apply_cgroup_limit_to_pid(cgroup_path, child_pid) != 0) {
        log_error("failed apply_cgroup_limit_to_pid");
        return -1;
    }

    //发送任意消息解除子进程的阻塞
    close(pipe_fd[0]);  //这里的关闭一定要放在创建子进程后面, 如果放在创建子进程前面, 由于继承关系,直接给子进程的读关闭了
    char cmds[] = "start";
    if (write(pipe_fd[1], cmds, strlen(cmds)) < 0) {
        perror("write");
    }
    close(pipe_fd[1]);


    if (waitpid(child_pid, NULL, 0) == -1) {
        perror("waitpid");
    }

    log_info("main exit");

    log_info("clean cgroup %s", cgroup_path);
    if (rmdir(cgroup_path) != 0) {
        log_error("failed remove cgroup: %s", cgroup_path);
        perror("failed remove cgroup");
    }
}

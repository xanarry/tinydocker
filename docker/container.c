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
#include <unistd.h>
#include <errno.h>
#include "../logger/log.h"

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

    char **cmds = (char **) args;
    log_info("start to run %s", cmds[0]);
    int ret = execv(cmds[0], cmds);
    if (ret != 0)
        perror("exec error");
    return 0;
}

int run(int it, char *image, char **args) {
    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD, args);
    if (child_pid == -1) {
        perror("clone subprocess error");
    }
    log_info("docker process pid=%d", child_pid);
   
    if (waitpid(child_pid, NULL, 0) == -1) {
        perror("waitpid");
    }

    log_info("main exit");
    /*
    log_info("clean cgroup %s", cgroup_path);
    if (rmdir(cgroup_path) != 0) {
        log_error("failed remove cgroup: %s", cgroup_path);
        perror("failed remove cgroup");
    }*/
}

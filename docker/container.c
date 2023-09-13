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
#include "container.h"
#include "../cmdparser/cmdparser.h"
#include "../logger/log.h"
#include "cgroup/cgroup.h"
#include "../util/utils.h"

extern char **environ;

//设置容器新的根目录
int init_and_set_new_root(char *new_root) {
    /* Ensure that 'new_root' and its parent mount don't have shared propagation (which would cause pivot_root() to
       return an error), and prevent propagation of mount events to the initial mount namespace. 
       保证new_root与它的父挂载点没有共享传播属性, 否则调用pivot_root会报错
       */
    if (mount(NULL, "/", NULL, MS_REC | MS_PRIVATE, NULL) == -1) {
        log_error("set / mount point as private error: %s", strerror(errno));
        return -1;
    }

     /* 保证'new_root'是一个独立挂载点*/
    if (mount(new_root, new_root, NULL, MS_BIND, NULL) == -1) {
        log_error("mount %s as new point error: %s", new_root, strerror(errno));
    }
    log_info("finish --bind mount '%s' to %s", new_root, new_root);

    char old_root[256] = {0};
    int slen = strlen(new_root);
    if (new_root[slen - 1] == '/') {
        new_root[slen - 1] = 0;
    }
    sprintf(old_root, "%s/%s", new_root, "old_root");
    if (!folder_exist(old_root)) {
        if (make_path(old_root) == -1) {
            log_error("failed to create old_root: %s", old_root);
            return -1;
        }
    }

    log_info("set new root as: %s, old_root save to: %s", new_root, old_root);

    if (syscall(SYS_pivot_root, new_root, old_root) != 0) {
        log_error("SYS_pivot_root new: %s, old: %s error: %s", new_root, old_root, strerror(errno));
        return -1;
    }

    chdir("/");

    // 卸载老的root
    char old_new[100] = "/old_root";
    int ret = umount2(old_new, MNT_DETACH);
    if (ret != 0) {
        log_error("failed to umount old_root: %s", old_root);
        return ret;
    }

    if (remove_dir(old_new) == -1) {
        log_info("failed to remove old root dir");
        return -1;
    }

    //检查proc目录挂载进程信息
    if (!folder_exist("/proc"))
        make_path("/proc");

    if (mount("proc", "/proc", "proc", MS_NOEXEC|MS_NOSUID|MS_NODEV, NULL) == -1) {
        perror("mount proc error");
        return -1;
    }
    return 0;
}


//创建只读层, 即镜像目录
int create_readonly_layer(char *image, char *readonly_dir) {
    // 传入的镜像就是一个目录, 直接使用
    log_info("create_readonly_layer %s", image);
    if (folder_exist(image)) {
        sprintf(readonly_dir, "%s", image);
        return 0;
    }

    //假定传入的都是tar文件, 解压到指定的目录
    char *image_hash = calculate_sha256(image);
    log_info("imageg hash:%s", image_hash);
    sprintf(readonly_dir, "%s/images/%s", TINYDOCKER_RUNTIME_DIR, image_hash);

    //如果传入的tar包已经解压到镜像目录, 直接复用
    if (folder_exist(readonly_dir)) {
        return 0;
    }

    make_path(readonly_dir); //创建镜像目录, 并且解压tar包
    return extract_tar(image, readonly_dir);
}


//创建读写层, 即overlayfs的upperdir
int create_readwrite_layer(char *container_name, char *readwrite_dir) {
    sprintf(readwrite_dir, "%s/containers/%s/upperdir", TINYDOCKER_RUNTIME_DIR, container_name);
    
    int t = make_path(readwrite_dir);
    log_info("create_readwrite_layer %s", readwrite_dir);
    return t;
}

//创建挂载点, 即容器实际的工作目录
int create_workespace_mount_point(char *container_name, char *lowerdir, char *upperdir, char *mountpoint) {
    char workdir[128] = {0};
    sprintf(workdir, "%s/containers/%s/workdir", TINYDOCKER_RUNTIME_DIR, container_name);     
    if (make_path(workdir) != 0) {
        return -1;
    }
    //mount -t overlay overlay -o lowerdir=lower,upperdir=upper,workdir=worker_dir overlay_dir/
    char data[512] = {0};
    sprintf(data, "lowerdir=%s,upperdir=%s,workdir=%s", lowerdir, upperdir, workdir);
    return mount("overlay", mountpoint, "overlay", MS_MGC_VAL, data);
}



int init_container_workerspace(struct docker_run_arguments *args, char *mountpoint) {
    // 如果是文件夹, 直接用这个文件作为只读层, 否则检查镜像是不是tar包, 然后解压
    char *readonly_dir = (char *) malloc(128 * sizeof(char));
    if (create_readonly_layer(args->image, readonly_dir) != 0) {
        return -1;
    }
    log_info("readonly_dir=%s", readonly_dir);

    char *readwrite_dir = (char *) malloc(128 * sizeof(char));
    if (create_readwrite_layer(args->name, readwrite_dir) != 0) {
        return -1;
    }
    log_info("readwrite_dir=%s", readwrite_dir);

    sprintf(mountpoint, "%s/containers/%s/mountpoint", TINYDOCKER_RUNTIME_DIR, args->name);
    if (make_path(mountpoint) != 0) {
        return -1;
    }
    log_info("mountpoint=%s", mountpoint);

    return create_workespace_mount_point(args->name, readonly_dir, readwrite_dir, mountpoint);
}


static char child_stack[8 * 1024 * 1024];
int pipe_fd[2];

int child_fn(void *args) {
    struct docker_run_arguments *run_args = (struct docker_run_arguments *) args;
    log_info("start init inner process");
    if (init_and_set_new_root(run_args->image) != 0) {
        log_error("init docker error");
        exit(-1);
    }

    //这里阻塞等收到父进程的命令后才开始运行, 为的是等待父进程设置cgroup
    close(pipe_fd[1]);
    char input_buf[1024] = {0};
    int len = read(pipe_fd[0], input_buf, 1024);
    close(pipe_fd[0]);

    //开始运行用户命令
    char **cmds = (char **) run_args->container_argv;
    log_info("start to run %s", cmds[0]);
    int ret = execv(cmds[0], cmds);
    if (ret != 0)
        perror("exec error");
    return 0;
}

int docker_run(struct docker_run_arguments *args) {
    char *mountpoint = (char *) malloc(128 * sizeof(char));
    if (init_container_workerspace(args, mountpoint) == -1) {
        log_error("failed to init_container_workerspace");
        return -1;
    }
    log_info("create overlay filesystem: %s", mountpoint);
    args->image = mountpoint;


    char cgroup_path[256] = {0};
    int ret = init_cgroup(args->name, cgroup_path); //创建一个test容器
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

    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD, args);
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

    //umount工作目录
    char work_dir[256] = {0};
    sprintf(work_dir, "%s/containers/%s/mountpoint", TINYDOCKER_RUNTIME_DIR, args->name);
    umount(work_dir);
    log_info("finish unmount container mountpoint %s", work_dir);

    //清理工作目录
    memset(work_dir, 0, 256);
    sprintf(work_dir, "%s/containers/%s", TINYDOCKER_RUNTIME_DIR, args->name);
    if (remove_dir(work_dir) == -1) {
        log_info("clean container %s work dir error", work_dir);
    }
    log_info("finish clean container work dir: %s", work_dir);

    //删除cgroup文件
    if (rmdir(cgroup_path) != 0) {
        log_error("failed remove cgroup: %s", cgroup_path);
        perror("failed remove cgroup");
    }
    log_info("finish clean cgroup %s", cgroup_path);
}

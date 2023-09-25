#define _GNU_SOURCE
#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include "../logger/log.h"
#include "../util/utils.h"
#include "cgroup.h"
#include "container.h"
#include "volumes.h"
#include "workspace.h"
#include "status_info.h"


extern char **environ;

//初始化docker运行说需要的目录
int init_docker_env() {
    if (!path_exist(TINYDOCKER_RUNTIME_DIR)) {
        make_path(TINYDOCKER_RUNTIME_DIR);
    }

    if (!path_exist(CONTAINER_STATUS_INFO_DIR)) {
        make_path(CONTAINER_STATUS_INFO_DIR);
    }

    if (!path_exist(CONTAINER_LOG_DIR)) {
        make_path(CONTAINER_LOG_DIR);
    }
    return 0;
}

int container_exists(char *container_name) {
    //容器状态文件
    char status_path[1024] = {0};
    sprintf(status_path, "%s/container_info/%s", TINYDOCKER_RUNTIME_DIR, container_name);
    //容器cgroup
    char cgroup_path[1024] = {0};
    get_container_cgroup_path(container_name, cgroup_path);
    //容器工作目录
    char container_dir[256] = {0};
    sprintf(container_dir, "%s/containers/%s", TINYDOCKER_RUNTIME_DIR, container_name);

    // 检查容器是否已经存在, 已经存在就报错
    if (path_exist(cgroup_path) || path_exist(status_path) || path_exist(container_dir)) {
        return 1;
    }
    return 0;
}

void clean_container_runtime(char *container_name) {
    if (!container_exists(container_name)) {
        return;
    }
    //删除cgroup文件
    if (remove_cgroup(container_name) != 0) {
        log_error("failed remove cgroup: %s", container_name);
        perror("failed remove cgroup");
    }
    log_info("finish clean cgroup %s", container_name);

    //构造挂载点
    char mountpoint[1024] = {0};
    sprintf(mountpoint, "%s/containers/%s/mountpoint", TINYDOCKER_RUNTIME_DIR, container_name);

    //umount 挂载的卷
    struct container_info info;
    read_container_info(container_name, &info);
    struct volume_config *volumes = (struct volume_config *) malloc(sizeof(struct volume_config));
    for (int i = 0; i < info.volume_cnt; i++) { // 解析出挂载的卷列表
        volumes[i] = parse_volume_config(info.volumes[i]);
    }
    umount_volumes(mountpoint, info.volume_cnt, volumes);
    free(volumes);
    log_info("finish unmount mounted volumes");

    //umount 容器跟目录挂载点
    umount(mountpoint);
    log_info("finish unmount container mountpoint %s", mountpoint);

    //清理容器工作目录
    char container_dir[256] = {0};
    sprintf(container_dir, "%s/containers/%s", TINYDOCKER_RUNTIME_DIR, container_name);
    if (remove_dir(container_dir) == -1) {
        log_error("clean container %s work dir error", container_dir);
    }
    log_info("finish clean container dir: %s", container_dir);

     // 删除状态文件
    remove_status_info(container_name);
}

char **load_process_env(int pid, struct key_val_pair *user_envs, int user_env_cnt) {
    const int max_env_cnt = 256 + 1; //多一个放空指针
    char **envs = (char **) malloc(sizeof(char *) * max_env_cnt); //最多支持max_env_cnt个环境变量
    int env_idx = 0;

    // 复制父进程的环境变量
    if (getpid() == pid) {  //获取当前进程自己的环境变量, 直接使用environ变量
        for (int i = 0; env_idx < max_env_cnt && environ[i] != NULL; i++) {
            envs[env_idx++] = environ[i];
        }
    } else {
        char path[256];
        sprintf(path, "/proc/%d/environ", pid);
        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            log_error("failed to open environ file: %s, err:%s", path, strerror(errno));
            return 0;
        }

        int buf_size = 4096;
        char buffer[buf_size];
        int len = 0;
        char kv_line[buf_size];
        int kv_idx = 0;
        while ((len = read(fd, buffer, buf_size)) > 0) {
            for (int i = 0; i < len; i++) {
                if (buffer[i] == '\0') { // envion文件形式为k1=v1\0k2=v2\0, 且一定有\0结尾, 所以这里能访问所有变量
                    char *kv_pair = (char *) malloc(kv_idx + 2);
                    strcpy(kv_pair, kv_line);
                    envs[env_idx++] = kv_pair;
                    memset(kv_line, 0, buf_size);
                    kv_idx = 0;
                } else {
                    kv_line[kv_idx++] = buffer[i];
                }
            }
            memset(buffer, 0, buf_size);
        }
        close(fd);
    }

    // 复制用户设置的环境变量
    for (int i = 0; env_idx < max_env_cnt && i < user_env_cnt; i++) {
        char *key = user_envs[i].key;
        char *val = user_envs[i].val;
        char *env_kv = (char *) malloc(10 + strlen(key) + strlen(val));
        strcpy(env_kv, "");
        sprintf(env_kv, "%s=%s", key, val);
        envs[env_idx++] = env_kv;
    }
    envs[env_idx] = NULL;

    return envs;
}

static char child_stack[8 * 1024 * 1024];
int pipe_fd[2];

int child_fn(void *args) {
    struct docker_run_arguments *run_args = (struct docker_run_arguments *) args;
    //如果容器是后台运行, 将日志日志重定向到指定目录
    if (run_args->detach) {
        char log_file_path[128] = {0};
        sprintf(log_file_path, "%s/%s", CONTAINER_LOG_DIR, run_args->name);
        log_info("container log file: %s", log_file_path);
        int fd = open(log_file_path, O_WRONLY | O_CREAT | O_TRUNC | O_SYNC, 0644); // 开启O_SYNC标志, 不要使用缓存
        if (fd < 0) {
            log_error("failed to open container log file: %s", log_file_path);
        }
        dup2(fd, 1);
        dup2(fd, 2);
    }

    // 为什么放在这个地方, 因为如果pivot_root后如果拿指定进程的环境变量就会报找不到文件了, 因为进入了容器里面, 但这里其实无所谓, 因为是拿当前进程自己的
    char **envs = load_process_env(getpid(), run_args->env, run_args->env_cnt);

    log_info("start init inner process");
    if (init_and_set_new_root(run_args->mountpoint) != 0) {
        log_error("init docker error");
        exit(-1);
    }

    //这里阻塞等收到父进程的命令后才开始运行, 为的是等待父进程设置cgroup
    close(pipe_fd[1]);
    char input_buf[1024] = {0};
    read(pipe_fd[0], input_buf, 1024);
    close(pipe_fd[0]);

    //开始运行用户命令
    char **cmds = (char **) run_args->container_argv;
    log_info("start to run %s", cmds[0]);
    int ret = execve(cmds[0], cmds, envs);
    if (ret != 0)
        perror("exec error");
    return 0;
}

int docker_run(struct docker_run_arguments *args) {
    if (container_exists(args->name)) {
        log_error("container %s has exists", args->name);
        return -1;
    }
    char *mountpoint = (char *) malloc(128 * sizeof(char));
    if (init_container_workerspace(args, mountpoint) == -1) {
        log_error("failed to init_container_workerspace");
        return -1;
    }
    log_info("create overlay filesystem mountpoint: %s", mountpoint);
    args->mountpoint = mountpoint;

    if (mount_volumes(mountpoint, args->volume_cnt, args->volumes) == -1) {
        return -1;
    }

    int ret = init_cgroup(args->name); //创建一个test容器
    if (ret != 0) {
        perror("failed to init cgroup");
        return ret;
    }

    // 注意这里cpu和mem如果设置的太小, 容器可能起不来, cpu最小要求1000, 也就是1%, mem测试能起来的最小值是204800
    if (set_cgroup_limits(args->name, args->cpu, args->memory, NULL) != 0) {
        log_error("failed to set_cgroup_limits for %s", args->name);
        return -1; 
    }
    log_info("set_cgroup_limits cpu=%d, mem=%d", args->cpu, args->memory);


    if (pipe(pipe_fd) == -1)
        return -1;

    // 这里不要加CLONE_NEWUSER, 否则会导致pivot_root权限不足, 
    pid_t child_pid = clone(child_fn, child_stack+(8 * 1024 * 1024), CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC | SIGCHLD, args);
    if (child_pid == -1) {
        perror("clone subprocess error");
    }
    log_info("docker process pid=%d", child_pid);

    //应用cgroup限制
    if (apply_cgroup_limit_to_pid(args->name, child_pid) != 0) {
        log_error("failed apply_cgroup_limit_to_pid");
        return -1;
    }

    int start_time = time(NULL);
    struct container_info info = create_container_info(args, child_pid, CONTAINER_RUNNING, start_time);
    write_container_info(args->name, &info);

    //发送任意消息解除子进程的阻塞
    close(pipe_fd[0]);  //这里的关闭一定要放在创建子进程后面, 如果放在创建子进程前面, 由于继承关系,直接给子进程的读关闭了
    char cmds[] = "start";
    if (write(pipe_fd[1], cmds, strlen(cmds)) < 0) {
        perror("write");
    }
    close(pipe_fd[1]);

    // 如果配置了it任一参数, 等待容器退出
    if (args->detach == 1) {
        log_info("leave container process running in background");
        return 0;
    }

    int exit_status;
    if (waitpid(child_pid, &exit_status, 0) == -1) {
        perror("waitpid");
    }

    if (WIFSIGNALED(exit_status)) { //容器进程被信号杀死
        printf("constainer is killed by signal %d\n", WTERMSIG(exit_status));
    } else { //正常退出
        log_info("container process exit");
        update_container_info(args->name, CONTAINER_EXITED);
    }
    return 0;
}



int docker_commit(struct docker_commit_arguments *args) {
    // 检查容器是否存在
    char container_mountpoint[512] = {0};
    sprintf(container_mountpoint, "%s/containers/%s/mountpoint", TINYDOCKER_RUNTIME_DIR, args->container_name);
    if (path_exist(container_mountpoint) == 0) {
        log_error("container is not exists: %s", args->container_name);
        return -1;
    }
    
    char tar_path[512] ={0};
    if (args->tar_path == NULL) {
        sprintf(tar_path, "%s.tar", args->container_name);
        args->tar_path = tar_path;
    }

    if (create_tar(container_mountpoint, args->tar_path) != 0) {
        log_error("failed to commit container: %s", args->container_name);
        return -1;
    }

    return 0;
}


int docker_ps(struct docker_ps_arguments *args) {
    struct container_info *info_list = (struct container_info *) malloc(sizeof(struct container_info) * 128);
    int cnt = list_containers_info(info_list);
    char *titles[] = {"CONTAINER_ID", "IMAGE", "COMMAND", "CREATED", "STATUS", "NAMES", NULL};
    int spans[6] =   {12,              5,       7,         7,         6,        5}; //记录每个字段输出的最大宽度, 与titles一一对应
    for (int i = 0; i < cnt; i++) {
        if (args->list_all == 0 && strcmp(info_list[i].status, "RUNNING") != 0) {
            continue;
        }
        ssize_t container_id_len = strlen(info_list[i].container_id);
        spans[0] = container_id_len > spans[0] ? container_id_len : spans[0];

        ssize_t image_len = strlen(info_list[i].image);
        spans[1] = image_len > spans[1] ? image_len : spans[1];

        ssize_t command_len = strlen(info_list[i].command);
        spans[2] = command_len > spans[2] ? command_len : spans[2];

        spans[3] = 19; //2023-12-12 12:12:12这样的形式, 固定19长度
        spans[4] = 7; //RUNING|STOPPING|EXITED, 最长7

        ssize_t name_len = strlen(info_list[i].name);
        spans[5] = name_len > spans[5] ? name_len : spans[5];
    }

    for (int i = 0; titles[i] != NULL; i++) {
        printf("%-*s\t", spans[i], titles[i]);
    }
    printf("\n");
    for (int i = 0; i < cnt; i++) {
        if (args->list_all == 0 && strcmp(info_list[i].status, "RUNNING") != 0) {
            continue;
        }
        printf("%-*s\t%-*s\t%-*s\t%-*s\t%-*s\t%-*s\n", spans[0], info_list[i].container_id, spans[1], info_list[i].image, \
         spans[2], info_list[i].command, spans[3], info_list[i].created, spans[4], info_list[i].status, spans[5], info_list[i].name);
    }
    return 0;
}

int docker_top(struct docker_top_arguments *args) {
    int pid_list[4096];
    int pid_cnt = get_container_processes_id(args->container_name, pid_list);
    if (pid_cnt == -1) {
        log_error("failed to get container process list");
        return -1;
    }

    char *pid_str_list = (char *) malloc (sizeof(char *) * pid_cnt * 32);
    strcpy(pid_str_list, "");  // 清空结果字符串
    char str_pid[64];
    for (int i = 0; i < pid_cnt; i++) {
        printf("%d\n", pid_list[i]);
        strcpy(str_pid, "");  // 清空结果字符串
        sprintf(str_pid, "%d", pid_list[i]);
        strcat(pid_str_list, str_pid);
        strcat(pid_str_list, " ");
    }

    char *cmd = (char *) malloc (sizeof(char *) * (strlen(pid_str_list) + 32));
    sprintf(cmd, "ps -f -p %s", pid_str_list);
    log_info("cmd: %s", cmd);
    int ret = system(cmd);

    free(pid_str_list);
    free(cmd);
    return ret;
}


int docker_exec(struct docker_exec_arguments *args) {
    int current_pid = getpid();
    // 获取当前进程的cgroup路径， 用户还原父进程的cgroup, 否则当前运行进程也加入到了容器内部的cgroup
    char *cgroup_files[64];
    int cgroup_file_cnt = get_cgroup_files(current_pid, cgroup_files, 64);
    if (cgroup_file_cnt < 0) {
        log_error("failed to get current prcess cgroup, pid=%d", current_pid);
        exit(-1);
    }

    //启动第一个进程用来在启动用户程序后户还原当前exec进程本身的cgroup
    int pipe_fd[2];
    pipe(pipe_fd);
    int pid = fork();
    if (pid < 0) {
        exit(-1);
    } else if (pid == 0) {
        close(pipe_fd[1]);
        char read_buf[128] = {0};
        int len = read(pipe_fd[0], read_buf, 128); //如果父进程处理遇到错误会直接退出关闭管道，然后这里读的数据就是0，需要特殊处理
        log_info("cgroup help prcess get message: %s, message_len: %d", read_buf, len);
        //移除父进程的cgroup
        if (len > 0) {
            if (write_pid_to_cgroup_procs(current_pid, "/sys/fs/cgroup/user.slice/user-1000.slice/session-2.scope/cgroup.procs") == -1) {
                log_error("failed to moved out docker exec pid %d from new cgroup setting", current_pid);
            } else {
                log_info("docker exec pid %d moved out from new cgroup setting", current_pid);
            }
        }
        log_info("parrent process exit, exit myself, pid=%d", current_pid);
        exit(0);
    }

    // 先设置cgroup好让子进程继承, 不然在绑定了命名空间后会提示找不到cgroup文件
    if (apply_cgroup_limit_to_pid(args->container_name, current_pid) == -1) {
        log_error("failed to set cgroup limits");
        exit(-1);
    }

    // 找出目标容器中的一个进程ID, 该进程用来寻找ns文件
    int pid_list[4096];
    int pid_cnt = get_container_processes_id(args->container_name, pid_list);
    if (pid_cnt <= 0) {
        log_error("failed to get container process list");
        return -1;
    }
    //默认第一个进程为1号进程, 可能不准, 但是在我们这个简单环境下基本都是它了
    int one_pid = pid_list[0];

    // 记录容器1号进程的环境变量
    char **envs = load_process_env(one_pid, args->env, args->env_cnt);

    //设置当前主进程的命名空间    
    //CLONE_NEWUTS | CLONE_NEWPID | CLONE_NEWNS | CLONE_NEWNET | CLONE_NEWIPC
    char ns_file[1024];
    char *ns_typs[] = {"ipc", "uts", "net", "pid", "mnt", NULL}; 
    for (int i = 0; ns_typs[i] != NULL; i++) {
        // /proc/25032/ns/
        strcpy(ns_file, ""); //清空字符串
        sprintf(ns_file, "/proc/%d/ns/%s", one_pid, ns_typs[i]);
        int fd = open(ns_file, O_RDONLY | O_CLOEXEC);
        if (fd == -1 || setns(fd, 0) == -1) { // Join that namespace 
            log_error("failed to join current process to destinct %s ns", ns_typs[i]);
            exit(-1);
        }
        close(fd);
    }
    
    // 创建一个子进程运行用户程序
    pid = fork();
    if (pid < 0) {
        log_error("failed to create subprocess");
        exit(1);
    } else if (pid == 0) { //子进程
        printf("run %s\n", args->container_argv[0]);
        if (execve(args->container_argv[0], args->container_argv, envs) == -1) {  //Execute a command in namespace 
            log_error("failed to run cmd: %s", args->container_argv[0]);
        }
        exit(0);
    } else { // 父进程
        close(pipe_fd[0]);
        char buf[100] = "remove_crgroup";
        write(pipe_fd[1], buf, strlen(buf));
        close(pipe_fd[1]);

        if (args->detach == 0) {
            log_info("docker exec waiting user command to finish now, user cmd pid=%d", pid);
            waitpid(pid, NULL, 0);  // 等待子进程结束
        }
        log_info("docker exec finished");
    }

    return 0;
}

int docker_stop(struct docker_stop_arguments *args) {
    for (int c = 0; c < args->container_cnt; c++) {
        char *container_name = args->container_names[c];
        int pid_list[1024];
        int pid_cnt = get_container_processes_id(container_name, pid_list);
        for (int p = 0; p < pid_cnt; p++) {
            int ret = kill(pid_list[p], SIGTERM);
            log_info("send SIGTERM to pid %d in container %s ret %d", pid_list[p], container_name, ret);
        }
    }

    if (args->time > 0) {
        log_info("wait %d seconds to send SIGKILL", args->time);
        sleep(args->time); //睡眠等待时间, 如果还有容器进程没有停止, 直接kill掉
        for (int c = 0; c < args->container_cnt; c++) {
            char *container_name = args->container_names[c];
            int pid_list[1024];
            int pid_cnt = get_container_processes_id(container_name, pid_list);
            for (int p = 0; p < pid_cnt; p++) {
                int ret = kill(pid_list[p], SIGKILL);
                log_info("send SIGKILL to pid %d in container %s ret %d", pid_list[p], container_name, ret);
            }
        }
    }

    for (int i = 0; i < args->container_cnt; i++) {
        char *container_name = args->container_names[i];
        //将容器状态标记为stoped
        if (update_container_info(container_name, CONTAINER_STOPPED) == -1) {
            log_error("failed to set containter %s status as STOPPED", container_name);
        }
    }
    return 0;       
}


int docker_rm(struct docker_rm_arguments *args) {
    for (int i = 0; i < args->container_cnt; i++) {
        char *container_name = args->containers[i];
        if (!container_exists(container_name)) {
            log_error("container %s not exists, ignore it", container_name);
            continue;
        }

        struct container_info info;
        if (read_container_info(container_name, &info) == -1) {
            log_error("failed to load container's status for %s", container_name);
            return -1;
        }

        if (strcmp(info.status, "RUNNING") == 0) {
            log_warn("container %s running, ignore remove", container_name);
            continue;
        }

        clean_container_runtime(container_name);
        log_info("finish clean runtime dir for container: %s", container_name);
    }
    return 0;
}
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <string.h>
#include <errno.h>
#include "../util/utils.h"
#include "../logger/log.h"

static char *cgroup_base = "/sys/fs/cgroup/system.slice";

int init_cgroup(char *container_name, char *cgroup_path) {
    if (!path_exist(cgroup_base)) {
        log_error("cgroup_base: %s not exists", cgroup_base);
        return -1;
    }
   
    sprintf(cgroup_path, "%s/cdocker-%s", cgroup_base, container_name);
    if (path_exist(cgroup_path)) {
        return 0;
    }

    int ret = make_path(cgroup_path);
    if (ret == -1) {
        log_error("init cgroup error, failed to create %s", cgroup_path);
    }
    return ret;
}

int set_mem_limit(char *cgroup_path, int mem_max) {
    if (mem_max <= 0) {
        log_error("invalid mem_max value: %d", mem_max);
        return -1;
    }

    char memory[256] = {0};
    sprintf(memory, "%s/%s", cgroup_path, "memory.max");
    int fd = open(memory, O_WRONLY|O_TRUNC);
    if (fd == -1) {
        return -1;
    }

    char limitation[100] = {0};
    sprintf(limitation, "%d", mem_max);
    size_t len = write(fd, limitation, strlen(limitation));
    if (len == strlen(limitation)) {
        return 0;
    }
    close(fd);
    return -1;
}


int set_cpu_limit(char *cgroup_path, int cpu_time) {
    if (cpu_time < 1000) {
        log_error("invalid cpu.max value: %d", cpu_time);
        return -1;
    }

    char cpu[256] = {0};
    sprintf(cpu, "%s/%s", cgroup_path, "cpu.max");
    int fd = open(cpu, O_WRONLY|O_TRUNC);
    if (fd < 0) {
        log_error("failed to open cgroup file %s, err:%s", cpu, strerror(errno));
        return -1;
    }
    char limitation[100] = {0};
    sprintf(limitation, "%d %d", cpu_time, 100000);
    size_t len = write(fd, limitation, (size_t)strlen(limitation));
    if (len == strlen(limitation)) {
        return 0;
    }
    close(fd);
    return -1;
}


int set_cpuset_limit(char *cgroup_path, char *cpus) {
    if (cpus == NULL) {
        log_error("invalid cpuset.cpus value, can not be NULL");
        return -1;
    }

    char cpuset_cpus[256] = {0};
    sprintf(cpuset_cpus, "%s/%s", cgroup_path, "cpuset.cpus");
    int fd = open(cpuset_cpus, O_WRONLY|O_TRUNC);
    if (fd == -1) {
        return -1;
    }

    size_t len = write(fd, cpus, strlen(cpus));
    if (len == strlen(cpus)) {
        return 0;
    }
    close(fd);
    return -1;
}


int apply_cgroup_limit_to_pid(char *cgroup_path, int pid) {
    char procs[256] = {0};
    sprintf(procs, "%s/%s", cgroup_path, "cgroup.procs");
    int fd = open(procs, O_WRONLY|O_APPEND);
    if (fd < 0) {
        return fd;
    }

    char pid_str[100] = {0};
    sprintf(pid_str, "%d\n", pid);
    write(fd, pid_str, strlen(pid_str));
    close(fd);
    return 0;
}


int set_cgroup_limits(char *cgroup_path, int cpu, int memory, char *cpuset) {
    //设置内存限制
    if (memory > 0) {
        if (set_mem_limit(cgroup_path, memory) != 0) {
            log_error("failed set mem limit in cgroup");
            return -1;
        }
    }
    
    //设置cpu限制
    if (cpu >= 1000) {
        if (set_cpu_limit(cgroup_path, cpu) != 0) {
            log_error("failed set cpu limit in cgroup");
            return -1;
        }
    }

    //设置cpuset限制
    if (cpuset != NULL) {
        if (set_cpuset_limit(cgroup_path, cpuset) != 0) {
            log_error("failed set cpu set in cgroup");
            return -1;
        }
    }

    return 0;
}

int get_container_processes_id(char *container_name, char *pid_list) {
    char cgroup_procs_path[1024] = {0};
    sprintf(cgroup_procs_path, "%s/cdocker-%s/%s", cgroup_base, container_name, "cgroup.procs");
    if (!path_exist(cgroup_procs_path)) {
        log_error("can not find container by name: %s", container_name);
        return -1;
    }

    FILE *file = fopen(cgroup_procs_path, "r");
    if (file == NULL) {
        log_error("failed to open the file: %s", cgroup_procs_path);
        return -1;
    }

    char line[32];
    strcpy(pid_list, "");  // 清空结果字符串
    while (fgets(line, sizeof(line), file)) {
        strtok(line, "\n");  // 去除行尾的换行符
        strcat(pid_list, line);
        strcat(pid_list, " ");
    }

    fclose(file);
    return 0;
}
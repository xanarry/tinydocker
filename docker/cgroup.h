#ifndef CGROUP_H
#define CGROUP_H

int init_cgroup(char *container_name, char *cgroup_path);
int apply_cgroup_limit_to_pid(char *cgroup_path, int pid);
int set_cgroup_limits(char *cgroup_path, int cpu, int memory, char *cpuset);
int get_container_processes_id(char *container_name, char *pid_list);

#endif
#ifndef __CMD_PARSER_H__
#define __CMD_PARSER_H__

enum docker_command_type {
    DOCKER_RUN,
    DOCKER_COMMIT,
    DOCKER_PS,
    DOCKER_TOP,
    DOCKER_EXEC,
    DOCKER_STOP,
    DOCKER_RM,
    DOCKER_NETWORK_CREATE,
    DOCKER_NETWORK_LIST,
    DOCKER_NETWORK_RM,
    DOCKER_NETWORK_CONNECT,
    DOCKER_NETWORK_DISCONNECT,
};

struct key_val_pair {
    char *key;
    char *val;
};

struct volume_config {
    char host[50]; //主机路径
    char container[50]; //容器路径
    int ro; //只读:1, 读写:0, 错误配置:-1
};

struct docker_cmd {
    enum docker_command_type cmd_type;
    void *arguments;
};

struct docker_run_arguments {
    //Usage:  docker run [OPTIONS] IMAGE [COMMAND] [ARG...]
    int volume_cnt;
    struct volume_config *volumes;
    int interactive;
    int detach; 
    int tty;
    int cpu;
    int memory;
    char *image;
    char *name;
    int env_cnt;
    struct key_val_pair *env;
    int container_argc;
    char **container_argv; // arg0 arg1 ... arg127
    char *mountpoint; //运行时辅助参数
};


struct docker_commit_arguments {
    char *container_name;
    char *tar_path;
};


struct docker_ps_arguments {
    int list_all;
};


struct docker_top_arguments {
    char *container_name;
};

struct docker_stop_arguments {
    int time;
    int container_cnt;
    char **container_names; // arg0 arg1 ... arg127
};

struct docker_rm_arguments {
    int container_cnt;
    char **containers;
};


struct docker_exec_arguments {
    //Usage:  docker exec [OPTIONS] CONTAINER COMMAND [ARG...]
    int detach;
    int interactive;
    int tty;
    int env_cnt;
    struct key_val_pair *env;
    char *container_name;
    int container_argc;
    char **container_argv; // arg0 arg1 ... arg127
};


struct docker_network_create {
    char *name;
    char *cider;
};

struct docker_network_rm {
    int network_argc;
    char **network_argv; // arg0 arg1 ... arg127
};

struct docker_network_connect {
    //Usage:  docker network connect [OPTIONS] NETWORK CONTAINER
    char *network_name;
    char *container_name;
};



struct volume_config parse_volume_config(char* input);
struct docker_cmd parse_docker_cmd(int argc, char *argv[]);
void print_docker_cmds(struct docker_cmd);


#endif /* __CMD_PARSER_H__ */

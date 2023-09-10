#ifndef __CMD_PARSER_H__
#define __CMD_PARSER_H__

enum docker_command_type {
    DOCKER_RUN,
    DOCKER_STOP,
    DOCKER_EXEC,
    DOCKER_PS,
};

struct key_val_pair {
    char *key;
    char *val;
};

struct docker_cmd {
    enum docker_command_type cmd_type;
    void *arguments;
};

struct docker_run_arguments {
    //Usage:  docker run [OPTIONS] IMAGE [COMMAND] [ARG...]
    int volume_cnt;
    struct key_val_pair *volume;
    int interactive;
    int tty;
    int cpu;
    int memory;
    char *image;
    int container_argc;
    char **container_argv; // arg0 arg1 ... arg127
};

struct docker_exec_arguments {
    //Usage:  docker run [OPTIONS] IMAGE [COMMAND] [ARG...]
    int detach;
    int interactive;
    int tty;
    int env_cnt;
    struct key_val_pair *env;
    char *work_dir;
};


struct docker_cmd parse_docker_cmd(int argc, char *argv[]);
void docker_run_cmd_print(struct docker_run_arguments *a);

#endif /* __CMD_PARSER_H__ */

#include <stdlib.h>
#include <argp.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "cmdparser.h"

struct key_val_pair parse_key_val_pair(char *str) {
    struct key_val_pair pair = {NULL, NULL};

    char *token = strtok(str, ":");
    if (token != NULL) {
        pair.key = token;
        token = strtok(NULL, ":");
        if (token != NULL) {
            pair.val = token;
        }
    }

    return pair;
}

// 打印帮助信息
void print_cmd_help(const struct argp *argp) {
    if (argp->args_doc != NULL) {
        printf("%s\n", argp->args_doc);
    }
    printf("OPTIONS:\n");
    for (const struct argp_option *opt = argp->options; opt != NULL && opt->name; opt++) {
        printf("  -%c, --%s  %s\n", opt->key, opt->name != NULL ? opt->name : "unknown", opt->doc != NULL ? opt->doc : "unknown");
    }
    printf("\n");
}


/*====================docker run命令行======================*/
//参数说明
char *docker_run_doc = "Usage:  tinydocker run [OPTIONS] IMAGE [COMMAND] [ARG...]";

//参数配置
struct argp_option docker_run_option_setting[] = {	
    //{"--长参数", "-缩写参数", '提示值: --file=提示值', "flag", "说明文档" }
    { "volume",      'v', "k:v", 0, "设置卷" },
    { "name",        'n', "str", 0, "容器名字"},
    { "interactive", 'i', "false|true", OPTION_ARG_OPTIONAL, "开启交互模式" },
    { "tty",         't', "false|true", OPTION_ARG_OPTIONAL, "开启tty" },
    { "cpu-shares",  'c', "int_val", 0, "设置cpu限制, 必须大于1000" },
    { "memory",      'm', "int_val", 0, "设置内存限制" },
    { 0 }
};

struct volume_config parse_volume_config(char* input) {
    struct volume_config result;
    result.ro = 0; //默认为读写挂载
    char* token;
    int i = 0;

    // 使用strtok函数分割字符串
    token = strtok(input, ":");
    while (token != NULL) {
        if (i == 0) {
            strcpy(result.host, token);
        } else if (i == 1) {
            strcpy(result.container, token);
        } else if (i == 2) { // 指定了挂载模式, 且为只读
            result.ro = strcmp(token, "ro") == 0;
        }
        token = strtok(NULL, ":");
        i++;
    }

    // 如果输入的字符串只有a和b，将c设为空字符串
    if (i < 2) {
        result.ro = -1;
    }
    return result;
}

//参数解析函数
static error_t docker_run_parse_func(int key, char *arg, struct argp_state *state) {
    struct docker_run_arguments *arguments = state->input;
    //printf("==%c, [%s], %d, %d\n", key, arg, state->next, state->argc);
    if (arguments->image != NULL)
        return 0;

    switch (key) {
        case 'v':
            struct volume_config vol = parse_volume_config(arg);
            if (vol.ro == -1) {
                printf("卷参数配置错误, 请使用: host_dir:container_dir:ro|rw 的形式\n");
                print_cmd_help(state->root_argp);
                exit(-1);
            }
            arguments->volumes[arguments->volume_cnt++] = vol;
            break;
        case 'i':
            arguments->interactive = 1;
            break;
        case 't':
            arguments->tty = 1;
            break;
        case 'c':
            arguments->cpu = atoi(arg);
            break;
        case 'm':
            arguments->memory = atoi(arg);
            break;
        case 'n':
            arguments->name =arg;
            break;
        case ARGP_KEY_ARG:
            arguments->image = arg;
            for (int i = state->next; i < state->argc; i++) {
                arguments->container_argv[arguments->container_argc++] = state->argv[i];  
            }
            arguments->container_argv[arguments->container_argc] = NULL;
    }
    return 0;
}

int docker_run_cmd_check(struct docker_run_arguments *a) {
    if (a->image == NULL || (a->cpu != -1 && a->cpu < 1000) || (a->memory != -1 && a->memory == 0) || a->container_argc < 1) {
        return 0;
    }
    return 1;
}

// 打印命令参数
void docker_run_cmd_print(struct docker_run_arguments *a) {
    printf("interactive=%d\n", a->interactive);
    printf("tty=%d\n", a->tty);
    printf("cpu=%d\n", a->cpu);
    printf("memory=%d\n", a->memory);
    printf("image=%s\n", a->image);
    printf("name=%s\n", a->name);
    for (int i = 0; i < a->volume_cnt; i++) {
        printf("host_dir:%s, container_dir:%s, ro:%d\n", a->volumes[i].host, a->volumes[i].container, a->volumes[i].ro);
    }
    printf("args_count=%d\n", a->container_argc);
    for (int i = 0; a->container_argv[i]; i++) {
        printf("%s ", a->container_argv[i]);
    }
    puts("");
}


// ==============================docker exec===========================
//参数说明
char *docker_exec_doc = "Usage:  tinydocker exec [OPTIONS] CONTAINER COMMAND [ARG...]";

//参数配置
struct argp_option docker_exec_option_setting[] = {	
    //{"--长参数", "-缩写参数", '提示值: --file=提示值', "flag", "说明文档" }
    { "detach",      'd', "false|true", OPTION_ARG_OPTIONAL, "后台运行" },
    { "interactive", 'i', "false|true", OPTION_ARG_OPTIONAL, "开启交互模式" },
    { "tty",         't', "false|true", OPTION_ARG_OPTIONAL, "开启tty" },
    { "env",         'e', "int_val", 0, "环境变量" },
    { 0 }
};

//参数解析函数
static error_t docker_exec_parse_func(int key, char *arg, struct argp_state *state) {
    struct docker_exec_arguments *arguments = state->input;
    //printf("key=[%c], arg=[%s], %d, %d\n", key, arg, state->next, state->argc);
    if (arguments->container_name != NULL)
        return 0;

    switch (key) {
        case 'd':
            arguments->detach = 1;
            break;
        case 'i':
            arguments->interactive = 1;
            break;
        case 't':
            arguments->tty = 1;
            break;
        case 'e':
            arguments->env[arguments->env_cnt++] = parse_key_val_pair(arg);
            break;
        case ARGP_KEY_ARG:
            arguments->container_name = arg;
            for (int i = state->next; i < state->argc; i++) {
                arguments->container_argv[arguments->container_argc++] = state->argv[i];  
            }
            arguments->container_argv[arguments->container_argc] = NULL;
    }
    return 0;
}

// 打印命令参数
void docker_exec_cmd_print(struct docker_exec_arguments *a) {
    printf("interactive=%d\n", a->interactive);
    printf("tty=%d\n", a->tty);
    printf("detach=%d\n", a->detach);
    printf("container_name=%s\n", a->container_name);
    for (int i = 0; i < a->env_cnt; i++) {
        printf("env_key:%s, env_val:%s\n", a->env[i].key, a->env[i].val);
    }
    printf("args_count=%d\n", a->container_argc);
    for (int i = 0; a->container_argv[i]; i++) {
        printf("%s ", a->container_argv[i]);
    }
    puts("");
}



// ==============================docker commit===========================
void docker_commit_cmd_print(struct docker_commit_arguments *a) {
    printf("container_name=%s\n", a->container_name);
    printf("tar_path=%s\n", a->tar_path != NULL ? a->tar_path : "");
}


// ==============================docker ps===========================
void docker_ps_cmd_print(struct docker_ps_arguments *a) {
    printf("list_all=%d\n", a->list_all);
}


// ==============================docker top===========================
void docker_top_cmd_print(struct docker_top_arguments *a) {
    printf("container=%s\n", a->container_name);
}

struct docker_cmd parse_docker_cmd(int argc, char *argv[]) {
    if (argc < 2) {
        printf("请输入有效的docker命令\n");
        exit(-1);
    }
    char *action = argv[1];
    if (strcmp(action, "run") == 0) {
        struct argp docker_run_argp = {docker_run_option_setting, docker_run_parse_func, docker_run_doc, NULL};
        struct docker_run_arguments *arguments = (struct docker_run_arguments *) malloc(sizeof(struct docker_run_arguments));
        arguments->volumes = (struct volume_config *) malloc(128 * sizeof(struct volume_config));
        arguments->image = NULL;
        arguments->name = malloc(128 * sizeof(char *));
        arguments->cpu = -1;
        arguments->memory = -1;
        arguments->container_argc = 0;
        arguments->container_argv = malloc(128 * sizeof(char *));
        sprintf(arguments->name, "%ld", time(NULL)); //默认容器名字使用当前的时间戳

        argp_parse(&docker_run_argp, argc - 1, argv + 1, ARGP_IN_ORDER | ARGP_NO_ERRS, 0, arguments);
        //docker_run_cmd_print(&arguments);
        if (!docker_run_cmd_check(arguments)) {
            print_cmd_help(&docker_run_argp);
            exit(-1);
        }
        struct docker_cmd result = {.cmd_type=DOCKER_RUN, .arguments=arguments};
        return result;
    }

    if (strcmp(action, "commit") == 0) {
        //docker commit container_name path(可选项)
        struct docker_commit_arguments *arguments = (struct docker_commit_arguments *) malloc(sizeof(struct docker_commit_arguments));
        arguments->container_name = NULL;
        arguments->tar_path = NULL; 

        if (argc == 3) {
            arguments->container_name = argv[2];
        } else if (argc == 4) {
            arguments->container_name = argv[2];
            arguments->tar_path = argv[3];
        } else {
            printf("Usage:  docker commit [OPTIONS] CONTAINER [REPOSITORY[:TAG]]\n");
            exit(-1);
        }
        struct docker_cmd result = {.cmd_type=DOCKER_COMMIT, .arguments=arguments};
        return result;
    }

     if (strcmp(action, "ps") == 0) {
       //docker ps -a(可选项)
        struct docker_ps_arguments *arguments = (struct docker_ps_arguments *) malloc(sizeof(struct docker_ps_arguments));
        arguments->list_all = 0;
        if (argc == 3) {
            if (strcmp(argv[2], "-a") == 0) {
                arguments->list_all = 1;
            }
        } else if (argc != 2) {
            printf("Usage:  docker ps [OPTIONS]\n");
            exit(-1);
        }
        struct docker_cmd result = {.cmd_type=DOCKER_PS, .arguments=arguments};
        return result;
    }

    if (strcmp(action, "top") == 0) {
        struct docker_top_arguments *arguments = (struct docker_top_arguments *) malloc(sizeof(struct docker_top_arguments));
        arguments->container_name = NULL;
        if (argc == 3) {
            arguments->container_name = argv[argc - 1];
        } else {
            printf("Usage:  docker top CONTAINER [ps OPTIONS]\n");
            exit(-1);
        }
        struct docker_cmd result = {.cmd_type=DOCKER_TOP, .arguments=arguments};
        return result;
    }

    if (strcmp(action, "exec") == 0) {
        struct argp docker_exec_argp = {docker_exec_option_setting, docker_exec_parse_func, docker_exec_doc, NULL};
        struct docker_exec_arguments *arguments = (struct docker_exec_arguments *) malloc(sizeof(struct docker_exec_arguments));
        arguments->detach = 0;
        arguments->interactive = 0;
        arguments->tty = 0;
        arguments->env_cnt = 0;
        arguments->env = (struct key_val_pair *) malloc(128 * sizeof(struct key_val_pair));
        arguments->container_name = NULL;
        arguments->container_argc = 0;
        arguments->container_argv = malloc(128 * sizeof(char *));
        argp_parse(&docker_exec_argp, argc - 1, argv + 1, ARGP_IN_ORDER | ARGP_NO_ERRS, 0, arguments);
        if (arguments->container_name == NULL || arguments->container_argc == 0) {
            print_cmd_help(&docker_exec_argp);
            exit(-1);
        }
        struct docker_cmd result = {.cmd_type=DOCKER_EXEC, .arguments=arguments};
        return result;
    }
}




void print_docker_cmds(struct docker_cmd cmds) {
    switch (cmds.cmd_type)
    {
    case DOCKER_RUN:
        docker_run_cmd_print(cmds.arguments);
        break;
    case DOCKER_COMMIT:
        docker_commit_cmd_print(cmds.arguments);
        break;
    case DOCKER_PS:
        docker_ps_cmd_print(cmds.arguments);
        break;
    case DOCKER_EXEC:
        docker_exec_cmd_print(cmds.arguments);
        break;
    }
}
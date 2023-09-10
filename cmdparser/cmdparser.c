#include <stdlib.h>
#include <argp.h>
#include <stdlib.h>
#include <string.h>
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
    printf("%s\n", argp->args_doc);
    for (const struct argp_option *opt = argp->options; opt->name != NULL; opt++) {
        printf("  -%c, --%s  %s\n", opt->key, opt->name, opt->doc);
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
    { "interactive", 'i', "false|true", OPTION_ARG_OPTIONAL, "开启交互模式" },
    { "tty",         't', "false|true", OPTION_ARG_OPTIONAL, "开启tty" },
    { "cpu-shares",  'c', "int_val", 0, "设置cpu限制, 必须大于1000" },
    { "memory",      'm', "int_val", 0, "设置内存限制" },
    { 0 }
};

//参数解析函数
static error_t docker_run_parse_func(int key, char *arg, struct argp_state *state) {
    struct docker_run_arguments *arguments = state->input;
    //printf("==%c, [%s], %d, %d\n", key, arg, state->next, state->argc);
    if (arguments->image != NULL)
        return 0;

    switch (key) {
        case 'v':
            arguments->volume[arguments->volume_cnt++] = parse_key_val_pair(arg);
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
        case ARGP_KEY_ARG:
            arguments->image = arg;
            for (int i = state->next; i < state->argc; i++) {
                arguments->args[arguments->args_count++] = state->argv[i];  
            }
            arguments->args[arguments->args_count] = NULL;
    }
    return 0;
}

int docker_run_cmd_check(struct docker_run_arguments *a) {
    if (a->image == NULL || (a->cpu != -1 && a->cpu < 1000) || (a->memory != -1 && a->memory == 0) || a->args_count < 1) {
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
    for (int i = 0; i < a->volume_cnt; i++) {
        printf("host_vol:%s, container_vol:%s\n", a->volume[i].key, a->volume[i].val);
    }
    printf("args_count=%d\n", a->args_count);
    for (int i = 0; a->args[i]; i++) {
        printf("%s ", a->args[i]);
    }
    puts("");
}


// ==============================docker exec===========================
//参数说明
char *docker_exec_doc = "Usage:  tinydocker exec [OPTIONS] CONTAINER COMMAND [ARG...]";

//参数配置
struct argp_option docker_exec_option_setting[] = {	
    //{"--长参数", "-缩写参数", '提示值: --file=提示值', "flag", "说明文档" }
    { "detach",      'd', "k:v", 0, "后台运行" },
    { "interactive", 'i', "false|true", OPTION_ARG_OPTIONAL, "开启交互模式" },
    { "tty",         't', "false|true", OPTION_ARG_OPTIONAL, "开启tty" },
    { "env list",    'e', "int_val", 0, "环境变量" },
    { "workdir str", 'w', "int_val", 0, "工作目录" },
    { 0 }
};

//参数解析函数
static error_t docker_exec_parse_func(int key, char *arg, struct argp_state *state) {
    struct docker_exec_arguments *arguments = state->input;
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
        case 'w':
            arguments->work_dir = arg;
            break;
    }
    return 0;
}

// 打印命令参数
void docker_exec_cmd_print(struct docker_exec_arguments *a) {
    printf("interactive=%d\n", a->interactive);
    printf("tty=%d\n", a->tty);
    printf("detach=%d\n", a->detach);
    printf("worker_dir=%s\n", a->work_dir);
    for (int i = 0; i < a->env_cnt; i++) {
        printf("env_key:%s, env_val:%s\n", a->env[i].key, a->env[i].val);
    }
    puts("");
}



struct docker_cmd parse_docker_cmd(int argc, char *argv[]) {
    char *action = argv[1];
    if (strcmp(action, "run") == 0) {
        struct argp docker_run_argp = {docker_run_option_setting, docker_run_parse_func, docker_run_doc, NULL};
        struct docker_run_arguments arguments = {
            .args_count=1,
            .volume = (struct key_val_pair *) malloc(128 * sizeof(struct key_val_pair)),
            .image=NULL,
            .cpu = -1,
            .memory = -1,
            .args_count=0,
            .args = malloc(128 * sizeof(char *))
        };
        argp_parse(&docker_run_argp, argc - 1, argv + 1, ARGP_IN_ORDER | ARGP_NO_ERRS, 0, &arguments);
        //docker_run_cmd_print(&arguments);
        if (!docker_run_cmd_check(&arguments)) {
            print_cmd_help(&docker_run_argp);
            exit(-1);
        }
        struct docker_cmd result = {.cmd_type=DOCKER_RUN, .arguments=&arguments};
        return result;
    }


    if (strcmp(action, "exec") == 0) {
        struct argp docker_exec_argp = {docker_exec_option_setting, docker_exec_parse_func, docker_exec_doc, NULL};
        struct docker_exec_arguments arguments = {
            .env_cnt = 0,
            .env = (struct key_val_pair *) malloc(128 * sizeof(struct key_val_pair))
        };
        argp_parse(&docker_exec_argp, argc - 1, argv + 1, ARGP_IN_ORDER, 0, &arguments);
        //docker_exec_cmd_print(&arguments);
        //print_cmd_help(&docker_exec_argp);
        struct docker_cmd result = {.cmd_type=DOCKER_EXEC, .arguments=&arguments};
        return result;
    }
}
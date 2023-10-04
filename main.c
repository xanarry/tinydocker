#include <stdlib.h>
#include <argp.h>
#include "logger/log.h"
#include "docker/container.h"
#include "cmdparser/cmdparser.h"
#include "docker/network.h"

int main(int argc, char **argv)
{
    //delte_network(TINYDOCKER_DEFAULT_NETWORK_NAME);
    //return 0;
    init_docker_env();
    struct docker_cmd result = parse_docker_cmd(argc, argv);
    switch (result.cmd_type)
    {
    case DOCKER_RUN:
        print_docker_cmds(result);
        docker_run(result.arguments);
        break;
    case DOCKER_COMMIT:
        print_docker_cmds(result);
        docker_commit(result.arguments);
        break;
    case DOCKER_PS:
        print_docker_cmds(result);
        docker_ps(result.arguments);
        break;
    case DOCKER_TOP:
        print_docker_cmds(result);
        docker_top(result.arguments);
        break;
    case DOCKER_EXEC:
        print_docker_cmds(result);
        docker_exec(result.arguments);
        break;
    case DOCKER_STOP:
        print_docker_cmds(result);
        docker_stop(result.arguments);
        break;
    case DOCKER_RM:
        print_docker_cmds(result);
        docker_rm(result.arguments);
        break;
    case DOCKER_NETWORK_CREATE:
        print_docker_cmds(result);
        struct docker_network_create *cmd = (struct docker_network_create *) result.arguments;
        create_network(cmd->name, cmd->cider, "bridge");
        break;
    case DOCKER_NETWORK_LIST:
        list_network();
        break;
    case DOCKER_NETWORK_RM:
        print_docker_cmds(result);
        remove_docker_network(result.arguments);
        break;
    default:
        puts("not support");
        exit(-1);
    }

    return 0;
}


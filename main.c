#include <stdlib.h>
#include <argp.h>
#include "logger/log.h"
#include "docker/container.h"
#include "cmdparser/cmdparser.h"

int main(int argc, char **argv)
{
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
    }

    return 0;
}


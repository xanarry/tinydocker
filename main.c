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
    }
    return 0;
}




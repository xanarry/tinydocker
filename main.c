#include <stdlib.h>
#include <argp.h>
#include "logger/log.h"
#include "docker/container.h"
#include "cmdparser/cmdparser.h"

int main(int argc, char **argv)
{
    struct docker_cmd result = parse_docker_cmd(argc, argv);
    if (result.cmd_type == DOCKER_RUN) {
        //docker_run_cmd_print(result.arguments);
        docker_run(result.arguments);
    }
    return 0;
}




#define _GNU_SOURCE
#include <sys/mount.h>
#include <unistd.h>
#include <stdlib.h>
#include <sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <limits.h>
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ftw.h>
#include <openssl/evp.h>
#include <limits.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include "logger/log.h"

#define TINYDOCKER_RUNTIME_DIR "/home/xanarry/tinydocker_runtime"


int path_exist(const char *path) {
    if (access(path, F_OK) == 0)
        return 1;
    return 0;
}

int make_path(const char *dir) {
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (!path_exist(tmp) && mkdir(tmp, S_IRWXU) == -1) {
                return -1;
            }
            *p = '/';
        }
    }
    return path_exist(tmp) ? 0 : mkdir(tmp, S_IRWXU);
}


struct container_info1 {
    char *container_id; //容器ID
    char *image; //容器镜像
    char *command; //容器运行的命令
    int created; //容器创建时间
    int status; //容器状态
    char *name; //容器名字
};


enum container_status {
    CONTAINER_RUNNING,
    CONTAINER_STOPPED,
    CONTAINER_EXITED
};

struct container_info {
    char container_id[128]; //容器ID
    char image[128]; //容器镜像
    char command[128]; //容器运行的命令
    char created[20]; //容器创建时间
    char status[10]; //容器状态
    char name[128]; //容器名字
};

#define MAX_BUF 1024
#define MAX_CGROUPS 10
#define CGROUP_ROOT "/sys/fs/cgroup"


int main(int argc, char const *argv[])
{
    char *cgroup_files[64];
    int cnt = get_cgroup_files(1, cgroup_files, 64);
    for (int i = 0; i < cnt; i++) {
        printf("%s\n", cgroup_files[i]);
    }
    return 0;
}

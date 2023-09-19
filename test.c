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
        char path[256] = "test.txt";
        sprintf(path, "/proc/%d/environ", 56440);

        int fd = open(path, O_RDONLY);
        if (fd < 0) {
            perror("Error opening file");
            return 0;
        }
        char *envs[256];
        int env_idx = 0;

        int buf_size = 10;
        char buffer[buf_size];
        int len = 0;
        char kv_line[4096];
        int kv_idx = 0;
        while ((len = read(fd, buffer, buf_size)) > 0) {
            for (int i = 0; i < len; i++) {
                if (buffer[i] == '\0') { // envion文件形式为k1=v1\0k2=v2\0, 且一定有\0结尾, 所以这里能访问所有变量
                    char *kv_pair = (char *) malloc(kv_idx + 2);
                    strcpy(kv_pair, kv_line);
                    envs[env_idx++] = kv_pair;
                    memset(kv_line, 0, 4096);
                    kv_idx = 0;
                } else {
                    kv_line[kv_idx++] = buffer[i];
                }
            }
            memset(buffer, 0, buf_size);
        }
        close(fd);

        for (int i = 0; i < env_idx; i++) {
            printf("%s\0",envs[i]);
        }
}

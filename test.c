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
#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#include <stdlib.h>
#include <ftw.h>
#include <openssl/evp.h>
#include <limits.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>


int folder_exist(char *folder) {
    struct stat sb;
    return (stat(folder, &sb) == 0 && S_ISDIR(sb.st_mode)) ? 1 : 0;
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
            if (!folder_exist(tmp) && mkdir(tmp, S_IRWXU) == -1) {
                return -1;
            }
            *p = '/';
        }
    }
    return  folder_exist(tmp) ? 0 : mkdir(tmp, S_IRWXU);
}



int main(int argc, char const *argv[]) {
    char *mountpoint = "/home/xanarry/tinydocker_runtime";
    char ro[256] = {0};
    sprintf(ro, "%s/ro", mountpoint);
    make_path(ro);
    printf("============================%d\n", folder_exist(ro));

    int t = mount("/home/xanarry/Downloads/busybox-1.36.0/scripts", ro, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL);
    printf("mount ret %d:\n", t);

    char cmd[1024] = {0};
    
    sprintf(cmd, "mount --bind -o ro %s %s", "/home/xanarry/Downloads/busybox-1.36.0/scripts", ro);
    system(cmd);
    int d;
    scanf("%d", &d);

    //system("mount --bind -o ro /home/xanarry/tinydocker_runtime/containers/tar_test12/mountpoint/ro /home/xanarry/tinydocker_runtime/containers/tar_test12/mountpoint/ro");

    //system("mount --bind -o ro /home/xanarry/Downloads/busybox-1.36.0/scripts /home/xanarry/tinydocker_runtime/containers/tar_test9/mountpoint/ro");
    //system("rm /home/xanarry/tinydocker_runtime/containers/tar_test10/mountpoint/ro/tt.t");
    // log_info("/home/xanarry/Downloads/busybox-1.36.0/scripts to %s", ro);

    // if (mount("/home/xanarry/Downloads/busybox-1.36.0/configs", rw, "bind", MS_BIND, NULL) != 0) {
    //     log_error("failed to mount volume %s:", rw);
    //     return 1;
    // }
    // log_info("/home/xanarry/Downloads/busybox-1.36.0/configs to %s", rw);

    umount(ro);

}

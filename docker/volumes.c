#include <sys/mount.h>
#include <unistd.h>
#include <string.h>
#include "../util/utils.h"
#include "../logger/log.h"
#include "volumes.h"

int mount_volumes(char *mountpoint, int vol_cnt, struct volume_config *vol_config) {
    // 先检查挂载的源目录是否都存在
    for (int i = 0; i < vol_cnt; i++) {
        if (path_exist(vol_config[i].host) != 1) {
            log_error("mounting host dir is not exist: %s", vol_config[i].host);
            return -1;
        }
    }

    // 按顺序挂载目录到容器工作目录
    char container_dir[1024] = {0};
    for (int i = 0; i < vol_cnt; i++) {
        struct volume_config vol = vol_config[i];
        memset(container_dir, 0, 1024 * sizeof(char));
        // 创建容器目录
        sprintf(container_dir, "%s/%s", mountpoint, vol.container);
        if (make_path(container_dir) != 0) {
            log_error("failed to create volume mount dir for %s", vol.container);
            return -1;
        }

        // 挂载目录
        if (mount(vol.host, container_dir, NULL, MS_BIND, NULL) == -1) {
            log_error("failed to mount volume from %s to %s", vol.host, vol.container);
            return -1;
        }

        // 设置只读挂载
        if (vol.ro == 1) {
            if (mount(container_dir, container_dir, NULL, MS_REMOUNT | MS_BIND | MS_RDONLY, NULL) == -1) {
                log_error("failed to set mount volume %s as readonly", vol.container);
                return -1;
            }
        }
    }
    return 0;
}


int umount_volumes(char *mountpoint, int vol_cnt, struct volume_config *vol_config) {
    // 按顺序卸载挂载到容器中的目录
    char container_dir[1024] = {0};
    for (int i = 0; i < vol_cnt; i++) {
        struct volume_config vol = vol_config[i];
        memset(container_dir, 0, 1024 * sizeof(char));
        // 生成容器目录中的挂载点
        if (vol.container[0] == '/') {
            sprintf(container_dir, "%s%s", mountpoint, vol.container);
        } else {
            sprintf(container_dir, "%s/%s", mountpoint, vol.container);
        }
        // 卸载挂载点
        if (umount(container_dir) == -1) {
            log_error("failed to mount volume from %s to %s", vol.host, vol.container);
        }
        log_info("umount volume %s", container_dir);
    }
    return 0;
}

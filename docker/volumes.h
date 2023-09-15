#ifndef VOLUMES_H
#define VOLUMES_H

#include "../cmdparser/cmdparser.h"

int mount_volumes(char *mountpoint, int vol_cnt, struct volume_config *vol_config);

int umount_volumes(char *mountpoint, int vol_cnt, struct volume_config *vol_config);

#endif
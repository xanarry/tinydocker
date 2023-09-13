# tinydocker
c语言实现的一个简单docker, 支持cgroup v2, overlayfs

v1.0            Version 1.0, 开启基本的命名空间隔开与容器命令执行
v2.0            Version 2.0, 开启基本的命名空间隔开与容器命令执行
v3.0            Version 3.0, 支持容器新的设置根目录, 设置方式是将镜像地址填成一个本地目录
v4.0            Version 4.0, 支持容器overlayfs, 容器里面的变更不再影响挂载的根目录, 支持tar包作为镜像地址(即运行根目录)
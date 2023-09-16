# tinydocker
c语言实现的一个简单docker, 支持cgroup v2, overlayfs

v1.0            Version 1.0, 开启基本的命名空间隔开与容器命令执行
v2.0            Version 2.0, 添加对容器的cgroup资源限制, 支持cgroup v2, 可以限制cpu和内存
v3.0            Version 3.0, 支持容器新的设置根目录, 设置方式是将镜像地址填成一个本地目录
v4.0            Version 4.0, 支持容器overlayfs, 容器里面的变更不再影响挂载的根目录, 支持tar包作为镜像地址(即运行根目录)
v5.0            Version 5.0, 支持卷挂载(支持挂载多个卷, 支持为卷设置只读|读写特性) 测试命令: make && sudo ./tinydocker run -i -t  -v /home/xanarry/Downloads/busybox-1.36.0/scripts:rw_dir -v /home/xanarry/Downloads/busybox-1.36.0/include:ro_dir:ro /home/xanarry/Downloads/busybox.tar.xz /bin/sh
v6.0            Version 6.0, 支持docker commit, 将容器当前工作状态打包到tar文件

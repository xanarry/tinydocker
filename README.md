# tinydocker
c语言实现的一个简单docker, 支持cgroup v2, overlayfs

v1.0            Version 1.0, 开启基本的命名空间隔开与容器命令执行
v2.0            Version 2.0, 添加对容器的cgroup资源限制, 支持cgroup v2, 可以限制cpu和内存
v3.0            Version 3.0, 支持容器新的设置根目录, 设置方式是将镜像地址填成一个本地目录
v4.0            Version 4.0, 支持容器overlayfs, 容器里面的变更不再影响挂载的根目录, 支持tar包作为镜像地址(即运行根目录)
v5.0            Version 5.0, 支持卷挂载(支持挂载多个卷, 支持为卷设置只读|读写特性) 测试命令: make && sudo ./tinydocker run -i -t  -v /home/xanarry/Downloads/busybox-1.36.0/scripts:rw_dir -v /home/xanarry/Downloads/busybox-1.36.0/include:ro_dir:ro /home/xanarry/Downloads/busybox.tar.xz /bin/sh
v6.0            Version 6.0, 支持docker commit, 将容器当前工作状态打包到tar文件
v7.0            Version 7.0, 支持docker ps, 列出运行容器或者全部容器(参数-a)
v8.0            Version 8.0, 支持docker top, 列出容器中的全部进程
v9.0            Version 9.0, 支持docker exec, 添加进程到容器中运行
v10.0           Version 10.0, 支持docker stop以及docke rm, 运行前检查容器名是否存在
v11.0           Version 11.0, docker run, exec支持传入用户环境变量
v12.0           Version 12.0, 支持容器后台运行以及日志文件输出, tinydocker没有守护进程, 如果后台进程运行完退回了, 状态无法设置到exited

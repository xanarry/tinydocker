main: 
	gcc logger/log.c util/utils.c cmdparser/cmdparser.c docker/cgroup/cgroup.c docker/container.c main.c -lcrypto -o tinydocker

clean:
	rm *.o tinydocker
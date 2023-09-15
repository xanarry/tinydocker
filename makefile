main: 
	gcc logger/log.c util/utils.c cmdparser/cmdparser.c docker/volumes.c docker/cgroup.c docker/container.c main.c -lcrypto -o tinydocker

clean:
	rm tinydocker; rm a.out; rm *.o
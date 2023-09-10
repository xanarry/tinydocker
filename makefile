main: 
	gcc main.c docker/container.c logger/log.c cmdparser/cmdparser.c -o tinydocker

clean:
	rm *.o tinydocker
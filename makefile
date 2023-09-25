main: 
	gcc -Wall logger/*.c util/*.c cmdparser/*.c docker/*.c main.c -lcrypto -o tinydocker

clean:
	rm tinydocker; rm a.out; rm *.o
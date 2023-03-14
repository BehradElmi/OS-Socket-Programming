CC=gcc

serverI: server client


server: server.c
	$(CC) server.c -o server.out

client: client.c
	$(CC) client.c -o client.out

clean:
	rm -rf *.o *.out
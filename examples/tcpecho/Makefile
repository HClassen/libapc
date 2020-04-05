CC= gcc
CFLAGS= -W -std=c99
PT= -pthread
LIB= -L../../ -lapc

all: server client

server: server.c
	$(CC) $(CFLAGS) -o server server.c $(PT) $(LIB)

client: client.c
	$(CC) $(CFLAGS) -o client client.c $(PT) $(LIB)

clean:
	rm ./client ./server
CC=gcc
CFLAGS=-Wall -Wextra -Iinclude

SRC_SERVER=src/server.c src/network.c src/process_reader.c
SRC_CLIENT=src/client.c src/network.c src/ui.c src/main_ui.c src/actions.c src/restart.c

all: server client

server:
	$(CC) $(CFLAGS) $(SRC_SERVER) -o server 

client:
	$(CC) $(CFLAGS) $(SRC_CLIENT) -o client -lncurses

clean:
	rm -f server client
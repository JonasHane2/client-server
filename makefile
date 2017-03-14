CC=gcc
CFLAGS=-Wall -Wextra -std=c99

.PHONY: all clean run

all: klient server

klient: klient.c communication.c
	$(CC) $(CFLAGS) $^ -o $@

server: server.c communication.c
	$(CC) $(CFLAGS) $^ -o $@

clean:
	rm -f klient server

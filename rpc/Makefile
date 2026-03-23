CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2

all: server client

server: server.o common.o
	$(CC) $(CFLAGS) -o server server.o common.o

client: client.o common.o
	$(CC) $(CFLAGS) -o client client.o common.o

server.o: server.c common.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

clean:
	rm -f *.o server client
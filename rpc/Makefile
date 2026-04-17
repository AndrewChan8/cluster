CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2

all: server client

server: server.o common.o kv_store.o replication.o handlers.o
	$(CC) $(CFLAGS) -o server server.o common.o kv_store.o replication.o handlers.o

client: client.o common.o
	$(CC) $(CFLAGS) -o client client.o common.o

server.o: server.c common.h kv_store.h replication.h handlers.h server_context.h
	$(CC) $(CFLAGS) -c server.c

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

kv_store.o: kv_store.c kv_store.h
	$(CC) $(CFLAGS) -c kv_store.c

replication.o: replication.c replication.h common.h
	$(CC) $(CFLAGS) -c replication.c

handlers.o: handlers.c handlers.h kv_store.h replication.h common.h server_context.h
	$(CC) $(CFLAGS) -c handlers.c

clean:
	rm -f *.o server client
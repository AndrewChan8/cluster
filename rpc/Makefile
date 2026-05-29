CC = gcc
CFLAGS = -Wall -Wextra -Werror -pedantic -std=c11 -O2 -pthread -D_POSIX_C_SOURCE=200112L

all: server client

server: server.o common.o ledger.o replication.o handlers.o anti_entropy.o
	$(CC) $(CFLAGS) -o server server.o common.o ledger.o replication.o handlers.o anti_entropy.o -lcrypto
	
client: client.o common.o
	$(CC) $(CFLAGS) -o client client.o common.o

server.o: server.c common.h ledger.h replication.h handlers.h server_context.h anti_entropy.h

client.o: client.c common.h
	$(CC) $(CFLAGS) -c client.c

common.o: common.c common.h
	$(CC) $(CFLAGS) -c common.c

ledger.o: ledger.c ledger.h
	$(CC) $(CFLAGS) -c ledger.c

replication.o: replication.c replication.h common.h
	$(CC) $(CFLAGS) -c replication.c

handlers.o: handlers.c handlers.h ledger.h replication.h common.h server_context.h
	$(CC) $(CFLAGS) -c handlers.c

anti_entropy.o: anti_entropy.c anti_entropy.h ledger.h replication.h
	$(CC) $(CFLAGS) -c anti_entropy.c

clean:
	rm -f *.o server client
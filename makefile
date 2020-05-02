CC     = gcc
CFLAGS = -Wall -g -c

all: main.o chunk.o debug.o memory.o
	$(CC) main.o chunk.o debug.o memory.o -o nqq

main.o: main.c common.h chunk.h debug.h
	$(CC) main.c $(CFLAGS) -o main.o

chunk.o: chunk.c chunk.h common.h memory.h
	$(CC) chunk.c $(CFLAGS) -o chunk.o

debug.o: debug.c debug.h chunk.h
	$(CC) debug.c $(CFLAGS) -o debug.o

memory.o: memory.c memory.h common.h
	$(CC) memory.c $(CFLAGS) -o memory.o

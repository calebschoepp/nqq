CC     = gcc
CFLAGS = -Wall -g -c

all: main.o chunk.o debug.o memory.o value.o
	$(CC) main.o chunk.o debug.o memory.o value.o -o nqq

main.o: main.c common.h chunk.h debug.h
	$(CC) main.c $(CFLAGS) -o main.o

chunk.o: chunk.c chunk.h common.h memory.h value.h
	$(CC) chunk.c $(CFLAGS) -o chunk.o

debug.o: debug.c debug.h chunk.h value.h
	$(CC) debug.c $(CFLAGS) -o debug.o

memory.o: memory.c memory.h common.h
	$(CC) memory.c $(CFLAGS) -o memory.o

value.o: value.c value.h memory.h common.h
	$(CC) value.c $(CFLAGS) -o value.o

clean:
	@rm -f nqq main.o chunk.o debug.o memory.o value.o

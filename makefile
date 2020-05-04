CC     = gcc
CFLAGS = -Wall -g -c

all: main.o chunk.o debug.o memory.o value.o vm.o compiler.o scanner.o
	$(CC) main.o chunk.o debug.o memory.o value.o vm.o compiler.o scanner.o -o nqq

main.o: main.c common.h chunk.h debug.h vm.h
	$(CC) main.c $(CFLAGS) -o main.o

chunk.o: chunk.c chunk.h common.h memory.h value.h
	$(CC) chunk.c $(CFLAGS) -o chunk.o

debug.o: debug.c debug.h chunk.h value.h
	$(CC) debug.c $(CFLAGS) -o debug.o

memory.o: memory.c memory.h common.h
	$(CC) memory.c $(CFLAGS) -o memory.o

value.o: value.c value.h memory.h common.h
	$(CC) value.c $(CFLAGS) -o value.o

vm.o: vm.c vm.h chunk.h common.h debug.h value.h
	$(CC) vm.c $(CFLAGS) -o vm.o

compiler.o: compiler.c compiler.h common.h scanner.h vm.h debug.h
	$(CC) compiler.c $(CFLAGS) -o compiler.o

scanner.o: scanner.c scanner.h common.h
	$(CC) scanner.c $(CFLAGS) -o scanner.o

clean:
	@rm -f nqq main.o chunk.o debug.o memory.o value.o vm.o compiler.o scanner.o

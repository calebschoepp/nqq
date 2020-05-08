CC     = gcc
CFLAGS = -Wall -g -c

all: main.o chunk.o debug.o memory.o value.o vm.o compiler.o scanner.o object.o table.o
	$(CC) main.o chunk.o debug.o memory.o value.o vm.o compiler.o scanner.o object.o table.o -o nqq

main.o: main.c common.h chunk.h debug.h vm.h
	$(CC) main.c $(CFLAGS) -o main.o

chunk.o: chunk.c chunk.h common.h memory.h value.h
	$(CC) chunk.c $(CFLAGS) -o chunk.o

debug.o: debug.c debug.h chunk.h value.h
	$(CC) debug.c $(CFLAGS) -o debug.o

memory.o: memory.c memory.h common.h object.h vm.h
	$(CC) memory.c $(CFLAGS) -o memory.o

value.o: value.c value.h memory.h common.h object.h
	$(CC) value.c $(CFLAGS) -o value.o

vm.o: vm.c vm.h chunk.h common.h debug.h value.h object.h memory.h table.h
	$(CC) vm.c $(CFLAGS) -o vm.o

compiler.o: compiler.c compiler.h common.h scanner.h vm.h debug.h object.h
	$(CC) compiler.c $(CFLAGS) -o compiler.o

scanner.o: scanner.c scanner.h common.h
	$(CC) scanner.c $(CFLAGS) -o scanner.o

object.o: object.c object.h common.h value.h memory.h vm.h table.h
	$(CC) object.c $(CFLAGS) -o object.o

table.o: table.c table.h memory.h object.h value.h common.h
	$(CC) table.c $(CFLAGS) -o table.o

clean:
	@rm -f nqq main.o chunk.o debug.o memory.o value.o vm.o compiler.o scanner.o object.o table.o

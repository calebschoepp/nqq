#include "common.h"
#include "chunk.h"
#include "debug.h"

int main(int argc, const char* argv[]) {
    Chunk chunk;
    initChunk(&chunk);

    // int constant = addConstant(&chunk, 1.354);
    // writeChunk(&chunk, OP_CONSTANT, 12);
    // writeChunk(&chunk, constant, 12);
    
    // writeChunk(&chunk, OP_RETURN, 12);
    // writeChunk(&chunk, OP_RETURN, 13);
    // writeChunk(&chunk, OP_RETURN, 13);
    // writeChunk(&chunk, OP_RETURN, 13);
    // writeChunk(&chunk, OP_RETURN, 13);
    // writeChunk(&chunk, OP_RETURN, 14);
    // writeChunk(&chunk, OP_RETURN, 16);
    // writeChunk(&chunk, OP_RETURN, 16);
    // writeChunk(&chunk, OP_RETURN, 17);
    // writeChunk(&chunk, OP_RETURN, 18);
    // writeChunk(&chunk, OP_RETURN, 19);
    for (int i = 1; i < 5; ++i) {
        writeConstant(&chunk, 1.242, i);
    }

    disassembleChunk(&chunk, "test chunk");
    freeChunk(&chunk);
    return 0;
}
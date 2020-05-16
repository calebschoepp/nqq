#include <stdlib.h>

#include "chunk.h"
#include "memory.h"

static void setLine(Chunk* chunk, int line) {
    if (chunk->firstLine == 0) {
        chunk->firstLine = line;
    }

    if (chunk->linesCapacity < line) {
        int oldLinesCapacity = chunk->linesCapacity;
        int potentialNewCapacity = GROW_CAPACITY(oldLinesCapacity);
        chunk->linesCapacity = line > potentialNewCapacity ? line : potentialNewCapacity;
        chunk->lines = GROW_ARRAY(chunk->lines, int, oldLinesCapacity, chunk->linesCapacity);
        for (int i = oldLinesCapacity; i < chunk->linesCapacity; ++i) {
            chunk->lines[i] = 0;
        }
    }

    chunk->lines[line - 1] = chunk->lines[line - 1] + 1;
}

void initChunk(Chunk* chunk) {
    chunk->count = 0;
    chunk->capacity = 0;
    chunk->code = NULL;
    chunk->linesCapacity = 0;
    chunk->lines = NULL;
    chunk->firstLine = 0;
    initValueArray(&chunk->constants);
}

void freeChunk(Chunk* chunk) {
    FREE_ARRAY(uint8_t, chunk->code, chunk->capacity);
    FREE_ARRAY(int, chunk->lines, chunk->linesCapacity);
    freeValueArray(&chunk->constants);
    initChunk(chunk);
}

void writeChunk(Chunk* chunk, uint8_t byte, int line) {
    if (chunk->capacity < chunk->count + 1) {
        int oldCapacity = chunk->capacity;
        chunk->capacity = GROW_CAPACITY(oldCapacity);
        chunk->code = GROW_ARRAY(chunk->code, uint8_t, oldCapacity, chunk->capacity);
    }

    chunk->code[chunk->count] = byte;
    setLine(chunk, line);
    chunk->count++;
}

int addConstant(Chunk* chunk, Value value) {
    writeValueArray(&chunk->constants, value);
    return chunk->constants.count - 1;
}

int getLine(Chunk* chunk, int offset) {
    int opCount = 0;
    int idx = 0;
    while (opCount <= offset) {
        opCount = opCount + chunk->lines[idx++];
    }

    return idx + chunk->firstLine - 1;
}
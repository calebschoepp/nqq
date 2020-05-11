#ifndef nqq_vm_h
#define nqq_vm_h

#include "chunk.h"
#include "table.h"
#include "value.h"

// ~65 KB
#define STACK_MAX 65536 // TODO dynamically grow stack or throw stack overflow error

typedef struct {
    Chunk* chunk;
    uint8_t* ip;
    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;

    Obj* objects;
} VM;

typedef enum {
    INTERPRET_OK,
    INTERPRET_COMPILE_ERROR,
    INTERPRET_RUNTIME_ERROR,
} InterpretResult;

extern VM vm;

void initVM();
void freeVM();
InterpretResult interpret(const char* source);
void push(Value value);
Value pop();

#endif
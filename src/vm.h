#ifndef nqq_vm_h
#define nqq_vm_h

#include "object.h"
#include "table.h"
#include "value.h"

#define FRAMES_MAX 64
#define STACK_MAX 65536
// TODO dynamically grow stack or throw stack overflow error

typedef struct {
    ObjFunction* function;
    uint8_t* ip;
    Value* slots;
} CallFrame;

// vm.nextOpWide
// 2 -> Was just set.
// 1 -> Next instruction should be wide. Is runtime error if no instruction uses this.
// 0 -> N/A.
// TODO optimize the 2 if statements out of the critical path
typedef struct {
    CallFrame frames[FRAMES_MAX];
    int frameCount;

    Value stack[STACK_MAX];
    Value* stackTop;
    Table globals;
    Table strings;
    uint8_t nextOpWide;

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
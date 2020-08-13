#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"
#include "native.h"

VM vm; // TODO pass as pointer to all functions instead of being static

static void resetStack() {
    vm.stackTop = vm.stack;
    vm.frameCount = 0;
    vm.openUpvalues = NULL;
}

static void runtimeError(const char* format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputs("\n", stderr);

    for (int i = vm.frameCount - 1; i >= 0; i--) {
        CallFrame* frame = &vm.frames[i];
        ObjFunction* function = frame->closure->function;
        // -1 because the IP is sitting on the next instruction to be
        // executed.
        size_t instruction = frame->ip - function->chunk.code - 1;
        fprintf(stderr, "[line %d] in ",
                function->chunk.lines[instruction]);
        if (function->name == NULL) {
        fprintf(stderr, "script\n");
        } else {
        fprintf(stderr, "%s()\n", function->name->chars);
        }
    }

    resetStack();
}

bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

void initVM() {
    resetStack();
    vm.objects = NULL;
    vm.bytesAllocated = 0;
    vm.nextGC = 1024 * 1024; // TODO tune this starting value

    vm.grayCount = 0;
    vm.grayCapacity = 0;
    vm.grayStack = NULL;

    vm.nextOpWide--;

    initTable(&vm.globals);
    initTable(&vm.strings);

    defineNatives(&vm);
}

void freeVM() {
    freeTable(&vm.globals);
    freeTable(&vm.strings);
    freeObjects();
}

void push(Value value) {
    *vm.stackTop = value;
    vm.stackTop++;
}

Value pop() {
    vm.stackTop--;
    return *vm.stackTop;
}

static Value peek(int distance) {
    return vm.stackTop[-1 - distance];
}

static bool call(ObjClosure* closure, int argCount) {
    if (argCount != closure->function->arity) {
        runtimeError("Expected %d arguments but got %d.",
            closure->function->arity, argCount);
        return false;
    }

    if (vm.frameCount == FRAMES_MAX) {
        runtimeError("Stack overflow.");
        return false;
    }
    CallFrame* frame = &vm.frames[vm.frameCount++];
    frame->closure = closure;
    frame->ip = closure->function->chunk.code;

    frame->slots = vm.stackTop - argCount - 1;
    return true;
}

static bool callValue(Value callee, int argCount) {
    if (IS_OBJ(callee)) {
        switch (OBJ_TYPE(callee)) {
            case OBJ_CLOSURE:
                return call(AS_CLOSURE(callee), argCount);
            case OBJ_NATIVE: {
                NativeFn native = AS_NATIVE(callee);
                Value result;
                char errMsg[50];
                bool err = native(argCount, vm.stackTop - argCount, &result, errMsg);
                if (err) {
                    runtimeError(errMsg);
                    return false;
                }
                vm.stackTop -= argCount + 1;
                push(result);
                return true;
            }
            default:
                // Non-callable object type.
                break;
        }
    }

    runtimeError("Can only call functions and classes.");
    return false;
}

static ObjUpvalue* captureUpvalue(Value* local) {
    ObjUpvalue* prevUpvalue = NULL;
    ObjUpvalue* upvalue = vm.openUpvalues;

    while (upvalue != NULL && upvalue->location > local) {
        prevUpvalue = upvalue;
        upvalue = upvalue->next;
    }

    if (upvalue != NULL && upvalue->location == local) return upvalue;

    ObjUpvalue* createdUpvalue = newUpvalue(local);
    createdUpvalue->next = upvalue;

    if (prevUpvalue == NULL) {
        vm.openUpvalues = createdUpvalue;
    } else {
        prevUpvalue->next = createdUpvalue;
    }

    return createdUpvalue;
}

static void closeUpvalues(Value* last) {
    while (vm.openUpvalues != NULL && vm.openUpvalues->location >= last) {
        ObjUpvalue* upvalue = vm.openUpvalues;
        upvalue->closed = *upvalue->location;
        upvalue->location = &upvalue->closed;
        vm.openUpvalues = upvalue->next;
    }
}

static void concatenate() {
    ObjString* b = AS_STRING(peek(0));
    ObjString* a = AS_STRING(peek(1));

    int length = a->length + b->length;
    char* chars = ALLOCATE(char, length + 1);
    memcpy(chars, a->chars, a->length);
    memcpy(chars + a->length, b->chars, b->length);
    chars[length] = '\0';

    ObjString* result = takeString(chars, length);
    pop();
    pop();
    push(OBJ_VAL(result));
}

static InterpretResult run() {
    CallFrame* frame = &vm.frames[vm.frameCount - 1];

#define READ_BYTE() (*frame->ip++)
#define READ_SHORT() \
    (frame->ip += 2, (uint16_t)((frame->ip[-2] << 8) | frame->ip[-1]))
#define READ_CONSTANT() (frame->closure->function->chunk.constants.values[READ_BYTE()])
#define READ_CONSTANT_SHORT() (frame->closure->function->chunk.constants.values[READ_SHORT()])
#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(valueType, op) \
    do { \
        if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) { \
            runtimeError("Operands must be numbers."); \
            return INTERPRET_RUNTIME_ERROR; \
        } \
        \
        double b = AS_NUMBER(pop()); \
        double a = AS_NUMBER(pop()); \
        push(valueType(a op b)); \
    } while (false)

    for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
        printf("          ");
        for (Value* slot = vm.stack; slot < vm.stackTop; slot++) {
            printf("[ ");
            printValue(*slot);
            printf(" ]");
        }
        printf("\n");
        disassembleInstruction(&frame->closure->function->chunk,
            (int)(frame->ip - frame->closure->function->chunk.code));
#endif
    uint8_t instruction = READ_BYTE();
    switch (instruction) {
        case OP_CONSTANT: {
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                Value constant = READ_CONSTANT_SHORT();
                push(constant);
            } else {
                Value constant = READ_CONSTANT();
                push(constant);
            }
            break;
        }
        case OP_NIL: push(NIL_VAL); break;
        case OP_TRUE: push(BOOL_VAL(true)); break;
        case OP_FALSE: push(BOOL_VAL(false)); break;
        case OP_POP: pop(); break;
        case OP_POP_N: {
            uint8_t popCount = READ_BYTE();
            for (int i = 0; i < popCount; i++) {
                pop();
            }
            break;
        }
        case OP_GET_LOCAL: {
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                uint16_t slot = READ_SHORT();
                push(frame->slots[slot]);
            } else {
                uint8_t slot = READ_BYTE();
                push(frame->slots[slot]);
            }
            break;
        }
        case OP_SET_LOCAL: {
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                uint16_t slot = READ_SHORT();
                frame->slots[slot] = peek(0);
            } else {
                uint8_t slot = READ_BYTE();
                frame->slots[slot] = peek(0);
            }
            break;
        }
        case OP_GET_GLOBAL: {
            ObjString* name;
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                name = AS_STRING(READ_CONSTANT_SHORT());
            } else {
                name = READ_STRING();
            }
            Value value;
            if (!tableGet(&vm.globals, OBJ_VAL(name), &value)) {
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            push(value);
            break;
        }
        case OP_DEFINE_GLOBAL: {
            ObjString* name;
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                name = AS_STRING(READ_CONSTANT_SHORT());
            } else {
                name = READ_STRING();
            }
            tableSet(&vm.globals, OBJ_VAL(name), peek(0));
            pop();
            break;
        }
        case OP_SET_GLOBAL: {
            ObjString* name;
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                name = AS_STRING(READ_CONSTANT_SHORT());
            } else {
                name = READ_STRING();
            }
            if (tableSet(&vm.globals, OBJ_VAL(name), peek(0))) {
                tableDelete(&vm.globals, OBJ_VAL(name));
                runtimeError("Undefined variable '%s'.", name->chars);
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_GET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            push(*frame->closure->upvalues[slot]->location);
            break;
        }
        case OP_SET_UPVALUE: {
            uint8_t slot = READ_BYTE();
            *frame->closure->upvalues[slot]->location = peek(0);
            break;
        }
        case OP_EQUAL: {
            Value b = pop();
            Value a = pop();
            push(BOOL_VAL(valuesEqual(a, b)));
            break;
        }
        case OP_GREATER:  BINARY_OP(BOOL_VAL, >); break;
        case OP_LESS:     BINARY_OP(BOOL_VAL, <); break;
        case OP_ADD: {
            if (IS_STRING(peek(0)) && IS_STRING(peek(1))) {
                concatenate();
            } else if (IS_NUMBER(peek(0)) && IS_NUMBER(peek(1))) {
                double b = AS_NUMBER(pop());
                double a = AS_NUMBER(pop());
                push(NUMBER_VAL(a + b));
            } else {
                runtimeError("Operands must be two numbers or two strings.");
                return INTERPRET_RUNTIME_ERROR;
            }
            break;
        }
        case OP_SUBTRACT: BINARY_OP(NUMBER_VAL, -); break;
        case OP_MULTIPLY: BINARY_OP(NUMBER_VAL, *); break;
        case OP_DIVIDE:   BINARY_OP(NUMBER_VAL, /); break;
        case OP_MODULO: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(fmod(a, b)));
            break;
        }
        case OP_NOT:
            push(BOOL_VAL(isFalsey(pop())));
            break;
        case OP_NEGATE:
            if (!IS_NUMBER(peek(0))) {
                runtimeError("Operand must be a number.");
                return INTERPRET_RUNTIME_ERROR;
            }

            push(NUMBER_VAL(-AS_NUMBER(pop())));
            break;
        case OP_POWER: {
            if (!IS_NUMBER(peek(0)) || !IS_NUMBER(peek(1))) {
                runtimeError("Operands must be numbers.");
                return INTERPRET_RUNTIME_ERROR;
            }
            double b = AS_NUMBER(pop());
            double a = AS_NUMBER(pop());
            push(NUMBER_VAL(pow(a, b)));
            break;
        }
        case OP_JUMP: {
            uint16_t offset = READ_SHORT();
            frame->ip += offset;
            break;
        }
        case OP_JUMP_IF_FALSE: {
            uint16_t offset = READ_SHORT();
            if (isFalsey(peek(0))) frame->ip += offset;
            break;
        }
        case OP_LOOP: {
            uint16_t offset = READ_SHORT();
            frame->ip -= offset;
            break;
        }
        case OP_CALL: {
            int argCount = READ_BYTE();
            if (!callValue(peek(argCount), argCount)) {
                return INTERPRET_RUNTIME_ERROR;
            }
            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
        case OP_CLOSURE: {
            // TODO handle 2 byte operand
            ObjFunction* function = AS_FUNCTION(READ_CONSTANT());
            ObjClosure* closure = newClosure(function);
            push(OBJ_VAL(closure));
            for (int i = 0; i < closure->upvalueCount; i++) {
                uint8_t isLocal = READ_BYTE();
                uint8_t index = READ_BYTE();
                if (isLocal) {
                    closure->upvalues[i] = captureUpvalue(frame->slots + index);
                } else {
                    closure->upvalues[i] = frame->closure->upvalues[index];
                }
            }
            break;
        }
        case OP_WIDE: {
            vm.nextOpWide = 2;
            break;
        }
        case OP_CLOSE_UPVALUE: {
            closeUpvalues(vm.stackTop - 1);
            pop();
            break;
        }
        case OP_BUILD_LIST: {
            // Before: [item1, item2, ..., itemN] After: [list]
            ObjList* list = newList();
            uint16_t itemCount;
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                itemCount = READ_SHORT();
            } else {
                itemCount = READ_BYTE();
            }

            push(OBJ_VAL(list)); // So that the list isn't sweeped by GC in appendToList
            for (int i = itemCount; i > 0; i--) {
                appendToList(list, peek(i));
            }
            pop();

            while (itemCount-- > 0) {
                pop();
            }
            push(OBJ_VAL(list));
            break;
        }
        case OP_BUILD_MAP: {
            // Before: [key1, value1, key2: value2, ..., keyN, valueN] After: [map]
            ObjMap* map = newMap();
            uint16_t itemCount;
            if (vm.nextOpWide == 1) {
                vm.nextOpWide--;
                itemCount = READ_SHORT();
            } else {
                itemCount = READ_BYTE();
            }

            push(OBJ_VAL(map)); // So that the map isn't sweeped by GC when table allocates
            int i = 2 * itemCount;
            while (i > 0) {
                Value key = peek(i--);
                Value value = peek(i--);

                if (!isHashable(key)) {
                    runtimeError("Map key is not hashable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                tableSet(&map->items, key, value);
            }
            pop();

            while (itemCount-- > 0) {
                pop();
                pop();
            }
            push(OBJ_VAL(map));
            break;
        }
        case OP_INDEX_SUBSCR: {
            // Before: [indexable, index] After: [index(indexable, index)]
            Value index = pop();
            Value indexable = pop();
            Value result;
            if (IS_LIST(indexable)) {
                ObjList* list = AS_LIST(indexable);
                if (!IS_NUMBER(index)) {
                    runtimeError("List index is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (!isValidListIndex(list, AS_NUMBER(index))) {
                    runtimeError("List index out of range.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                result = indexFromList(list, AS_NUMBER(index));
            } else if (IS_STRING(indexable)) {
                ObjString* string = AS_STRING(indexable);
                if (!IS_NUMBER(index)) {
                    runtimeError("String index is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (!isValidStringIndex(string, AS_NUMBER(index))) {
                    runtimeError("String index out of range.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                result = indexFromString(string, AS_NUMBER(index));
            } else if (IS_MAP(indexable)) {
                ObjMap* map = AS_MAP(indexable);
                if (!isHashable(index)) {
                    runtimeError("Map key is not hashable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                bool found = tableGet(&map->items, index, &result);
                if (!found) {
                    runtimeError("Key not found in map.");
                    return INTERPRET_RUNTIME_ERROR;
                }
            } else {
                runtimeError("Invalid type to index into.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(result);
            break;
        }
        case OP_STORE_SUBSCR: {
            // Before: [indexable, index, item] After: [item]
            Value item = pop();
            Value index = pop();
            Value indexable = pop();
            if (IS_LIST(indexable)) {
                ObjList* list = AS_LIST(indexable);
                if (!IS_NUMBER(index)) {
                    runtimeError("List index is not a number.");
                    return INTERPRET_RUNTIME_ERROR;
                } else if (!isValidListIndex(list, AS_NUMBER(index))) {
                    runtimeError("List index out of range.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                storeToList(list, AS_NUMBER(index), item);
            } else if (IS_MAP(indexable)) {
                ObjMap* map = AS_MAP(indexable);
                if (!isHashable(index)) {
                    runtimeError("Map key is not hashable.");
                    return INTERPRET_RUNTIME_ERROR;
                }
                tableSet(&map->items, index, item);
            } else {
                runtimeError("Can only store subscript in list or map.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(item);
            break;
        }
        case OP_RETURN: {
            Value result = pop();

            closeUpvalues(frame->slots);

            vm.frameCount--;
            if (vm.frameCount == 0) {
                pop();
                return INTERPRET_OK;
            }

            vm.stackTop = frame->slots;
            push(result);

            frame = &vm.frames[vm.frameCount - 1];
            break;
        }
    }
    if (vm.nextOpWide == 1) {
        runtimeError("OP_WIDE used on an invalid opcode.");
        return INTERPRET_RUNTIME_ERROR;
    } else if (vm.nextOpWide == 2) {
        vm.nextOpWide--;
    }
}

#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_CONSTANT_SHORT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult interpret(const char* source) {
    ObjFunction* function = compile(source);
    if (function == NULL) return INTERPRET_COMPILE_ERROR;

    push(OBJ_VAL(function));
    ObjClosure* closure = newClosure(function);
    pop();
    push(OBJ_VAL(closure));
    callValue(OBJ_VAL(closure), 0);

    return run();
}
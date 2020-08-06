#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "object.h"
#include "memory.h"
#include "vm.h"

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

static bool isFalsey(Value value) {
    return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// TODO use a macro to guard against wrong argCount

/*
Standard Library:
append, assert, clock, delete, input, len, num, print, write

Missing:
bool, string, list, map, set, slice, items, values, keys
*/

static bool appendNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Append a value to the end of a list increasing the list's length by 1
    if (argCount != 2) {
        *result = NIL_VAL;
        sprintf(errMsg, "append expected 2 argument but got %d.", argCount);
        return true;
    } else if (!IS_LIST(*args)) {
        *result = NIL_VAL;
        sprintf(errMsg, "append expected the first argument to be a list.");
        return true;
    }
    ObjList* list = AS_LIST(*args);
    Value item = *(args + 1);
    appendToList(list, item);
    *result = NIL_VAL;
    return false;
}

static bool assertNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Throw a runtime error if the argument does not evaluate to true
    if (argCount != 1) {
        *result = NIL_VAL;
        sprintf(errMsg, "assert expected 1 argument but got %d.", argCount);
        return true;
    }
    Value value = *args;
    if (isFalsey(value)) {
        *result = NIL_VAL;
        sprintf(errMsg, "failed assertion.");
        return true;
    }
    return false;
}

static bool clockNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return a double representing the number of seconds the process has been alive
    if (argCount != 0) {
        *result = NIL_VAL;
        sprintf(errMsg, "clock expected 0 arguments but got %d.", argCount);
        return true;
    }
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return false;
}

static bool deleteNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Delete an item from a list at the given index.
    // Every item past the deleted item has it's index decreased by 1.
    *result = NIL_VAL;
    if (argCount != 2) {
        sprintf(errMsg, "delete expected 2 argument but got %d.", argCount);
        return true;
    } if (!IS_LIST(*args)) {
        sprintf(errMsg, "delete expected the first argument to be a list.");
        return true;
    } else if (!IS_NUMBER(*(args + 1))) {
        sprintf(errMsg, "append expected the second argument to be a number.");
        return true;
    }

    ObjList* list = AS_LIST(*args);
    int index = AS_NUMBER(*(args + 1));

    if (!isValidListIndex(list, index)) {
        sprintf(errMsg, "index you are trying to delete is out of range.");
        return true;
    }

    deleteFromList(list, index);
    return false;
}

static bool inputNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return a string representing a line read from STDIN
    if (argCount != 0) {
        *result = NIL_VAL;
        sprintf(errMsg, "input expected 0 arguments but got %d.", argCount);
        return true;
    }
    int count = 0;
    int capacity = GROW_CAPACITY(0);
    char* input = NULL;
    input = GROW_ARRAY(input, char, 0, capacity);

    // Read in characters until we read 'enter'
    do {
        if (capacity < count + 1) {
            int oldCapacity = capacity;
            capacity = GROW_CAPACITY(oldCapacity);
            input = GROW_ARRAY(input, char, oldCapacity, capacity);
        }
        // Convoluted (void) and +1 to make compiler ignore return value
        (void)(scanf("%c", &input[count++])+1);
    } while (input[count - 1] != 0x0A);

    *result = OBJ_VAL(copyString(input, count - 1));
    FREE_ARRAY(char, input, count);
    return false;
}

static bool lenNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return the length of a string or list
    if (argCount != 1) {
        *result = NIL_VAL;
        sprintf(errMsg, "len expected 1 argument but got %d.", argCount);
        return true;
    }
    Value value = *args;
    if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        *result = NUMBER_VAL(string->length);
        return false;
    } else if (IS_LIST(value)) {
        ObjList* list = AS_LIST(value);
        *result = NUMBER_VAL(list->count);
        return false;
    } else {
        *result = NIL_VAL;
        sprintf(errMsg, "len expected a list or string.");
        return true;
    }
}

static bool numNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Attempt to convert a value into a number
    if (argCount != 1) {
        *result = NIL_VAL;
        sprintf(errMsg, "num expected 1 argument but got %d.", argCount);
        return true;
    }
    Value value = *args;
    if (IS_BOOL(value)) {
        if (isFalsey(value)) {
            *result = NUMBER_VAL(0);
        } else {
            *result = NUMBER_VAL(1);
        }
        return false;
    } if (IS_NUMBER(value)) {
        *result = value;
        return false;
    } if (IS_STRING(value)) {
        char* err;
        char* numString = AS_CSTRING(value);
        double number = strtod(numString, &err);
        if (*err != 0) {
            *result = NIL_VAL;
            sprintf(errMsg, "Cannot convert '%s' to a number.", numString);
            return true;
        }
        *result = NUMBER_VAL(number);
        return false;
    }
    // Should be unreachable down here
    *result = NIL_VAL;
    sprintf(errMsg, "num was passed an unexpected type.");
    return true;
}

static bool printNative(int argCount, Value* args, Value* result, char errMsg[]) {
    if (argCount != 1) {
        *result = NIL_VAL;
        sprintf(errMsg, "print expected 1 argument but got %d.", argCount);
        return true;
    }
    if (IS_STRING(*args)) {
        printf("%s", AS_CSTRING(*args));
    } else {
        printValue(*args);
    }
    printf("\n");
    *result = NIL_VAL;
    return false;
}

static bool writeNative(int argCount, Value* args, Value* result, char errMsg[]) {
    if (argCount != 1) {
        *result = NIL_VAL;
        sprintf(errMsg, "write expected 1 argument but got %d.", argCount);
        return true;
    }
    if (IS_STRING(*args)) {
        printf("%s", AS_CSTRING(*args));
    } else {
        printValue(*args);
    }
    *result = NIL_VAL;
    return false;
}

static void defineNative(const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm.globals, AS_STRING(vm.stack[0]), vm.stack[1]);
    pop();
    pop();
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

    defineNative("append", appendNative);
    defineNative("assert", assertNative);
    defineNative("clock", clockNative);
    defineNative("delete", deleteNative);
    defineNative("input", inputNative);
    defineNative("len", lenNative);
    defineNative("num", numNative);
    defineNative("print", printNative);
    defineNative("write", writeNative);
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
            if (!tableGet(&vm.globals, name, &value)) {
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
            tableSet(&vm.globals, name, peek(0));
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
            if (tableSet(&vm.globals, name, peek(0))) {
                tableDelete(&vm.globals, name);
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
            // Build a new list
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
        case OP_INDEX_SUBSCR: {
            // before: [indexable, index] after: [index(indexable, index)]
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
            } else {
                runtimeError("Invalid type to index into.");
                return INTERPRET_RUNTIME_ERROR;
            }
            push(result);
            break;
        }
        case OP_STORE_SUBSCR: {
            Value item = pop();
            Value index = pop();
            Value list = pop();
            if (!IS_LIST(list)) {
                runtimeError("Cannot store value in a non-list.");
                return INTERPRET_RUNTIME_ERROR;
            } else if (!IS_NUMBER(index)) {
                runtimeError("List index is not a number.");
                return INTERPRET_RUNTIME_ERROR;
            } else if (!isValidListIndex(AS_LIST(list), AS_NUMBER(index))) {
                runtimeError("Invalid list index.");
                return INTERPRET_RUNTIME_ERROR;
            }
            storeToList(AS_LIST(list), AS_NUMBER(index), item);
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
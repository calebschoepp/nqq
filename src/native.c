#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "memory.h"
#include "native.h"

/*
Standard Library:
append, assert, clock, delete, has, input, items, keys, len, num, print, values, write

Missing:
bool, string, list, map, set, slice
*/

#define VALIDATE_ARG_COUNT(funcName, numArgs) \
    do { \
        if (argCount != numArgs) { \
            *result = NIL_VAL; \
            sprintf(errMsg, #funcName " expected " #numArgs " arguments but got %d.", argCount); \
            return true; \
        } \
    } while (false)

static bool appendNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Append a value to the end of a list increasing the list's length by 1
    VALIDATE_ARG_COUNT(append, 2);
    if (!IS_LIST(*args)) {
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
    VALIDATE_ARG_COUNT(assert, 1);
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
    VALIDATE_ARG_COUNT(clock, 0);
    *result = NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
    return false;
}

static bool deleteNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Delete an item from a list or map
    *result = NIL_VAL;
    VALIDATE_ARG_COUNT(delete, 2);
    if (IS_LIST(*args)) {
        if (!IS_NUMBER(*(args + 1))) {
            sprintf(errMsg, "delete expected index to be a number for a list.");
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
    } else if (IS_MAP(*args)) {
        if (!isHashable(*(args + 1))) {
            sprintf(errMsg, "delete expected a hashable key for a map.");
            return true;
        }
        ObjMap* map = AS_MAP(*args);
        Value key = *(args + 1);
        tableDelete(&map->items, key);
        return false;
    } else {
        sprintf(errMsg, "delete expected the first argument to be a list or map.");
        return true;
    }
    // Shouldn't reach here
    return false;
}

static bool hasNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Determine if a list or map has a particular item
    *result = BOOL_VAL(false);
    VALIDATE_ARG_COUNT(has, 2);
    if (IS_LIST(*args)) {
        ObjList* list = AS_LIST(*args);
        Value item = *(args + 1);
        for (int i = 0; i < list->count; i++) {
            if (valuesEqual(item, list->items[i])) {
                *result = BOOL_VAL(true);
                break;
            }
        }
        return false;
    } else if (IS_MAP(*args)) {
        if (!isHashable(*(args + 1))) {
            sprintf(errMsg, "has expected item to be hashable.");
            return true;
        }
        ObjMap* map = AS_MAP(*args);
        Value item = *(args + 1);
        for (int i = 0; i < map->items.capacity; i++) {
            if (valuesEqual(item, map->items.entries[i].key)) {
                *result = BOOL_VAL(true);
                break;
            }
        }
        return false;
    } else {
        sprintf(errMsg, "has expected the first argument to be a list or map.");
        return true;
    }
    // Shouldn't reach here
    return false;
}

static bool inputNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return a string representing a line read from STDIN
    VALIDATE_ARG_COUNT(input, 0);
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

static bool itemsNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return a list of the items in a map where an item is a list of the key and value
    VALIDATE_ARG_COUNT(items, 1);
    if (!IS_MAP(*args)) {
        sprintf(errMsg, "items expected the first argument to be a map.");
        return true;
    }
    ObjMap* map = AS_MAP(*args);
    ObjList* itemsList = newList();
    Value itemsValue = OBJ_VAL(itemsList);
    push(itemsValue);

    for (int i = 0; i < map->items.capacity; i++) {
        if (map->items.entries[i].empty) {
            continue;
        }
        ObjList* kvPairList = newList();
        Value kvPairValue = OBJ_VAL(kvPairList);
        push(kvPairValue);
        appendToList(kvPairList, map->items.entries[i].key);
        appendToList(kvPairList, map->items.entries[i].value);
        appendToList(itemsList, kvPairValue);
        pop(kvPairValue);
    }

    pop();
    *result = itemsValue;
    return false;
}

static bool keysNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return a list of the keys in a map
    VALIDATE_ARG_COUNT(keys, 1);
    if (!IS_MAP(*args)) {
        sprintf(errMsg, "keys expected the first argument to be a map.");
        return true;
    }
    ObjMap* map = AS_MAP(*args);
    ObjList* keysList = newList();
    Value keysValue = OBJ_VAL(keysList);
    push(keysValue);

    for (int i = 0; i < map->items.capacity; i++) {
        if (map->items.entries[i].empty) {
            continue;
        }
        appendToList(keysList, map->items.entries[i].key);
    }

    pop();
    *result = keysValue;
    return false;
}

static bool lenNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return the length of a string or list
    VALIDATE_ARG_COUNT(len, 1);
    Value value = *args;
    if (IS_STRING(value)) {
        ObjString* string = AS_STRING(value);
        *result = NUMBER_VAL(string->length);
        return false;
    } else if (IS_LIST(value)) {
        ObjList* list = AS_LIST(value);
        *result = NUMBER_VAL(list->count);
        return false;
    } else if (IS_MAP(value)) {
        ObjMap* map = AS_MAP(value);
        int len = 0;
        for (int i = 0; i < map->items.capacity; i++) {
            if (!map->items.entries[i].empty) {
                len++;
            }
        }
        *result = NUMBER_VAL(len);
        return false;
    } else {
        *result = NIL_VAL;
        sprintf(errMsg, "len expected a list, string, or map.");
        return true;
    }
}

// TODO handle all edge cases here
static bool numNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Attempt to convert a value into a number
    VALIDATE_ARG_COUNT(num, 1);
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
    VALIDATE_ARG_COUNT(print, 1);
    if (IS_STRING(*args)) {
        printf("%s", AS_CSTRING(*args));
    } else {
        printValue(*args);
    }
    printf("\n");
    *result = NIL_VAL;
    return false;
}

static bool valuesNative(int argCount, Value* args, Value* result, char errMsg[]) {
    // Return a list of the values in a map
    VALIDATE_ARG_COUNT(values, 1);
    if (!IS_MAP(*args)) {
        sprintf(errMsg, "values expected the first argument to be a map.");
        return true;
    }
    ObjMap* map = AS_MAP(*args);
    ObjList* valuesList = newList();
    Value valuesValue = OBJ_VAL(valuesList);
    push(valuesValue);

    for (int i = 0; i < map->items.capacity; i++) {
        if (map->items.entries[i].empty) {
            continue;
        }
        appendToList(valuesList, map->items.entries[i].value);
    }

    pop();
    *result = valuesValue;
    return false;
}

static bool writeNative(int argCount, Value* args, Value* result, char errMsg[]) {
    VALIDATE_ARG_COUNT(write, 1);
    if (IS_STRING(*args)) {
        printf("%s", AS_CSTRING(*args));
    } else {
        printValue(*args);
    }
    *result = NIL_VAL;
    return false;
}

static void defineNative(VM* vm, const char* name, NativeFn function) {
    push(OBJ_VAL(copyString(name, (int)strlen(name))));
    push(OBJ_VAL(newNative(function)));
    tableSet(&vm->globals, vm->stack[0], vm->stack[1]);
    pop();
    pop();
}

void defineNatives(VM* vm) {
    defineNative(vm, "append", appendNative);
    defineNative(vm, "assert", assertNative);
    defineNative(vm, "clock", clockNative);
    defineNative(vm, "delete", deleteNative);
    defineNative(vm, "has", hasNative);
    defineNative(vm, "input", inputNative);
    defineNative(vm, "items", itemsNative);
    defineNative(vm, "keys", keysNative);
    defineNative(vm, "len", lenNative);
    defineNative(vm, "num", numNative);
    defineNative(vm, "print", printNative);
    defineNative(vm, "values", valuesNative);
    defineNative(vm, "write", writeNative);
}

#undef VALIDATE_ARG_COUNT

#ifndef nqq_object_h
#define nqq_object_h

#include "common.h"
#include "chunk.h"
#include "value.h"

#define OBJ_TYPE(value)         (AS_OBJ(value)->type)

#define IS_CLOSURE(value)       isObjType(value, OBJ_CLOSURE)
#define IS_FUNCTION(value)      isObjType(value, OBJ_FUNCTION)
#define IS_LIST(value)          isObjType(value, OBJ_LIST)
#define IS_NATIVE(value)        isObjType(value, OBJ_NATIVE)
#define IS_STRING(value)        isObjType(value, OBJ_STRING)

#define AS_CLOSURE(value)       ((ObjClosure*)AS_OBJ(value))
#define AS_FUNCTION(value)      ((ObjFunction*)AS_OBJ(value))
#define AS_LIST(value)          ((ObjList*)AS_OBJ(value))
#define AS_NATIVE(value)        (((ObjNative*)AS_OBJ(value))->function)
#define AS_STRING(value)        ((ObjString*)AS_OBJ(value))
#define AS_CSTRING(value)       (((ObjString*)AS_OBJ(value))->chars)

typedef enum {
    OBJ_CLOSURE,
    OBJ_FUNCTION,
    OBJ_LIST,
    OBJ_NATIVE,
    OBJ_STRING,
    OBJ_UPVALUE,
} ObjType;

// Base type all nqq objects inherit from
struct sObj {
    ObjType type;
    bool isMarked;
    struct sObj* next;
};

typedef struct {
    Obj obj;
    int arity;
    int upvalueCount;
    Chunk chunk;
    ObjString* name;
} ObjFunction;

typedef bool (*NativeFn) (int argCount, Value* args, Value* result, char errMsg[]);

typedef struct {
    Obj obj;
    NativeFn function;
} ObjNative;

// Note: hash is cached for strings because a) strings are immutable so the hash
// will never change b) string lookups are a very frequent action in Clox and
// we want it to be as quick as possible.
struct sObjString {
    Obj obj;
    int length;
    char* chars;
    uint32_t hash;
};

typedef struct sUpvalue {
    Obj obj;
    Value* location;
    Value closed;
    struct sUpvalue* next;
} ObjUpvalue;

typedef struct {
    Obj obj;
    ObjFunction* function;
    ObjUpvalue** upvalues;
    int upvalueCount;
} ObjClosure;

typedef struct {
    Obj obj;
    int count;
    int capacity;
    Value* items;
} ObjList;

ObjClosure* newClosure(ObjFunction* function);
ObjFunction* newFunction();
ObjNative* newNative(NativeFn function);
ObjString* takeString(char* chars, int length);
ObjString* copyString(const char* chars, int length);
Value indexFromString(ObjString* string, int index);
bool isValidStringIndex(ObjString* list, int index);
ObjUpvalue* newUpvalue(Value* slot);
ObjList* newList();
void appendToList(ObjList* list, Value value);
void storeToList(ObjList* list, int index, Value value);
Value indexFromList(ObjList* list, int index);
void deleteFromList(ObjList* list, int index);
bool isValidListIndex(ObjList* list, int index);
void printObject(Value value);

static inline bool isObjType(Value value, ObjType type) {
    return IS_OBJ(value) && AS_OBJ(value)->type == type;
}

#endif
#ifndef nqq_table_h
#define nqq_table_h

#include "common.h"
#include "value.h"

typedef struct {
    Value key;
    Value value;
    bool empty;
} Entry;

typedef struct {
    int count;
    int capacity;
    Entry* entries;
} Table;

void initTable(Table* table);
void freeTable(Table* table);
bool tableGet(Table* table, Value key, Value* value);
bool tableSet(Table* table, Value key, Value value);
bool tableDelete(Table* table, Value key);
ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash);
void tableRemoveWhite(Table* table);
void markTable(Table* table);
uint32_t hashBytes(uint8_t* key, int length);
bool isHashable(Value value);

#endif

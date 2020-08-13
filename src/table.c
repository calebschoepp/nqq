#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

#include <stdio.h>


#define TABLE_MAX_LOAD 0.75 // TODO tune this value

void initTable(Table* table) {
    table->count = 0;
    table->capacity = 0;
    table->entries = NULL;
}

void freeTable(Table* table) {
    FREE_ARRAY(Entry, table->entries, table->capacity);
    initTable(table);
}

static uint32_t hashValue(Value value) {
    // TODO
    return 0;
}

static Entry* findEntry(Entry* entries, int capacity, Value key) {
    uint32_t hash;
    if (IS_STRING(key)) {
        hash = AS_STRING(key)->hash;
    } else {
        hash = hashValue(key);
    }
    uint32_t index = hash % capacity;
    Entry* tombstone = NULL;

    for (;;) {
        Entry* entry = &entries[index];

        if (entry->empty) {
            if (IS_NIL(entry->value)) {
                // Emtpy entry
                return tombstone != NULL ? tombstone : entry;
            } else {
                // We found a tombstone
                if (tombstone == NULL) tombstone = entry;
            }
        } else if (valuesEqual(entry->key, key)) { // TODO is this an issue?
            // We found the key
            return entry;
        }

        index = (index + 1) % capacity;
    }
}

bool tableGet(Table* table, Value key, Value* value) {
    if (table->count == 0) return false;

    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->empty) return false;

    *value = entry->value;
    return true;
}

static void adjustCapacity(Table* table, int capacity) {
    Entry* entries = ALLOCATE(Entry, capacity);
    for (int i = 0; i < capacity; i++) {
        entries[i].key = NIL_VAL;
        entries[i].value = NIL_VAL;
        entries[i].empty = true;
    }

    table->count = 0;
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (entry->empty) continue;

        Entry* dest = findEntry(entries, capacity, entry->key);
        dest->key = entry->key;
        dest->value = entry->value;
        dest->empty = false;
        table->count++;
    }

    FREE_ARRAY(Entry, table->entries, table->capacity);
    table->entries = entries;
    table->capacity = capacity;
}

bool tableSet(Table* table, Value key, Value value) {
    if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
        int capacity = GROW_CAPACITY(table->capacity);
        adjustCapacity(table, capacity);
    }
    Entry* entry = findEntry(table->entries, table->capacity, key);

    bool isNewKey = entry->empty;
    if (isNewKey && IS_NIL(entry->value)) table->count++;

    entry->key = key;
    entry->value = value;
    entry->empty = false;
    return isNewKey;
}

bool tableDelete(Table* table, Value key) {
    if (table->count == 0) return false;

    // Find the entry
    Entry* entry = findEntry(table->entries, table->capacity, key);
    if (entry->empty) return false;

    // Place a tombstone in the entry
    entry->key = NIL_VAL;
    entry->value = BOOL_VAL(true);
    entry->empty = true;

    return true;
}

ObjString* tableFindString(Table* table, const char* chars, int length, uint32_t hash) {
    if (table->count == 0) return NULL;

    uint32_t index = hash % table->capacity;

    for (;;) {
        Entry* entry = &table->entries[index];

        if (entry->empty) {
            // Stop if we find an empty non-tombstone entry.
            if (IS_NIL(entry->value)) return NULL;
        } else if (IS_STRING(entry->key) && AS_STRING(entry->key)->length == length &&
            AS_STRING(entry->key)->hash == hash &&
            memcmp(AS_STRING(entry->key)->chars, chars, length) == 0) {
            // We found it.
            return AS_STRING(entry->key);
        }

        index = (index + 1) % table->capacity;
    }
}

void tableRemoveWhite(Table* table) {
    // TODO unclear if this is correct
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        if (!entry->empty && IS_OBJ(entry->key) && !AS_OBJ(entry->key)->isMarked) {
            tableDelete(table, entry->key);
        }
    }
}

void markTable(Table* table) {
    for (int i = 0; i < table->capacity; i++) {
        Entry* entry = &table->entries[i];
        markValue(entry->key);
        markValue(entry->value);
    }
}

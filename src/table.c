#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

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

uint32_t hashBytes(uint8_t* key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

bool isHashable(Value value) {
    if (IS_LIST(value) || IS_MAP(value)) {
        return false;
    }
    return true;
}

static uint32_t hashObject(Obj* obj) {
    // TODO confirm the hashes are fine and there are minimal collisions
    switch (obj->type) {
    case OBJ_CLOSURE: {
        return (uint32_t)((uint64_t)obj & 0x0000ffff);
        break;
    }
    case OBJ_FUNCTION: {
        return (uint32_t)((uint64_t)obj & 0x0000ffff);
    }
    case OBJ_LIST: {
        // Not hashable
        // Shouldn't reach here
        break;
    }
    case OBJ_NATIVE: {
        return (uint32_t)((uint64_t)obj & 0x0000ffff);
    }
    case OBJ_STRING: {
        // Cached
        // Shouldn't reach here
        break;
    }
    case OBJ_UPVALUE: {
        // Not a 1st class citizen
        // Shouldn't reach here
        break;
    }
    default:
        break;
    }
    // Shouldn't reach here
    return 0;
}

static uint32_t hashValue(Value value) {
    switch (value.type) {
    case VAL_BOOL: {
        if (AS_BOOL(value)) {
            return 1;
        } else {
            return 0;
        }
    }
    case VAL_NIL: {
        // TODO this may be the incorrect thing to hash to
        // TODO add a test case that the map {nil: 1, 2: 3} works fine
        return 2;
    }
    case VAL_NUMBER: {
        return (uint32_t)AS_NUMBER(value);
    }
    case VAL_OBJ: {
        return hashObject(AS_OBJ(value));
    }
    default:
        break;
    }
    // Shouldn't reach here
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
    // TODO check the key is hashable Value
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
    // TODO check the key is hashable Value
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
    // TODO check the key is hashable Value
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

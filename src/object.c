#include "object.h"

#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "table.h"
#include "value.h"
#include "vm.h"

static Obj* allocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    object->next = vm.objects;
    vm.objects = object;
    return object;
}

// allocate an object without storing it to the VM's object link lists
static Obj* xallocateObject(size_t size, ObjType type) {
    Obj* object = (Obj*)reallocate(NULL, 0, size);
    object->type = type;
    return object;
}

static uint32_t hashString(const char* key, int length) {
    uint32_t hash = 2166136261u;

    for (int i = 0; i < length; i++) {
        hash ^= key[i];
        hash *= 16777619;
    }

    return hash;
}

static ObjString* allocateString(char* chars, int length) {
    // size of an ObjString + number of characters + one extra
    // for null terminator
    int size = sizeof(ObjString) + sizeof(char) * (length + 1);
    ObjString* string = (ObjString*)allocateObject(size, OBJ_STRING);
    for (int i = 0; chars[i]; i++) {
        string->chars[i] = chars[i];
    }
    string->length = length;
    string->chars[length] = '\0';
    string->hash = hashString(string->chars, length);
    return string;
}

static void writeToString(ObjString* string, const char* chars) {
    for (int i = 0; i < string->length; i++) {
        string->chars[i] = chars[i];
    }
    string->chars[string->length] = '\0';
    string->hash = hashString(string->chars, string->length);
}

static void storeString(ObjString* string) {
    tableSet(&vm.strings, string, NIL_VAL);
    ((Obj*)string)->next = vm.objects;
    vm.objects = (Obj*)string;
}

// creates a string object without initializing the character array,
// doesn't check for interning and
// doesn't store it in the VM's object linked list
ObjString* xallocateString(int length) {
    int size = sizeof(ObjString) + sizeof(char) * (length + 1);
    ObjString* string = (ObjString*)xallocateObject(size, OBJ_STRING);
    string->length = length;
    string->chars[length] = '\0';
    return string;
}

// if the string is interned, assigns the string pointer
// to the interned string freeing it's original contents,
// else adds it as a new string to the intern table, and threads
// it in the VM's object list for GC.
void validateString(ObjString* string) {
    // 1. if the string is interned, return
    ObjString* interned = tableFindString(&vm.strings, string->chars,
                                          string->length, string->hash);
    if (interned != NULL) {
        // TODO: free the original string's memory
        string = interned;
    }
    storeString(string);
}

ObjString* takeString(char* chars, int length) {
    return allocateString(chars, length);
}

ObjString* copyString(const char* chars, int length) {
    // first creats an empty strings
    // then write the characters from the "chars" buffer
    // to the string's character array.
    // if a similar interned string is found, then free the string made
    // and return  the interned string instead.
    ObjString* temp = xallocateString(length);
    writeToString(temp, chars);
    ObjString* interned =
        tableFindString(&vm.strings, temp->chars, length, temp->hash);

    if (interned != NULL) {
        
        // TODO free string
        free(temp);
        return interned;
    }
    tableSet(&vm.strings, temp, NIL_VAL);
    return temp;
}

void printObject(Value value) {
    switch (OBJ_TYPE(value)) {
        case OBJ_STRING:
            printf("%s", AS_CSTRING(value));
            break;
    }
}
#ifndef clox_value_h
#define clox_value_h

#include "common.h"

typedef enum{
    VAL_BOOL,
    VAL_NIL,
    VAL_NUMBER
}ValueType;

typedef struct{
    ValueType type;
    union{
        bool boolean;
        double number;
    } as;
} Value;

// lifts the value from C's static land to Lox's dynamic land
#define BOOL_VAL(value) ((Value){VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){VAL_NUMBER, {.number = value}})


#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)


#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)


bool valuesEqual(Value a, Value b);

// value array

typedef struct {
    int capacity;
    int count;
    Value* values;
} ValueArray;


void initValueArray(ValueArray* array);
ValueArray* newValueArray();
void printValue(Value value);
void writeValueArray(ValueArray* array, Value value);
void freeValueArray(ValueArray* array);

// stack

typedef struct {
    Value* top;
    size_t size;
    Value* values;
}ValueStack;


void initValueStack(ValueStack* stack, size_t size);
void freeValueStack(ValueStack* stack);
Value popValue(ValueStack* stack);
void pushValue(ValueStack* stack, Value value);
void printValueStack(ValueStack* stack);


#endif
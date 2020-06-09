#include "compiler.h"

#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token previous;
    Token current;
    bool hadError;
    bool panicMode;
} Parser;

Parser parser;
Chunk* compilingChunk;

// operator precedence

// the enum values are in numerically
// increasing order because of how
// enums work

typedef enum {
    PREC_NONE,
    PREC_ASSIGN,      // =
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // ==
    PREC_COMPARISON,  // >= <= > <
    PREC_TERM,        // + -
    PREC_FACTOR,      // / * %
    PREC_UNARY,       // - !
    PREC_CALL,        // . ()
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)();

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

// helper functions

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error: ", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // do nothing
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }
    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) { errorAt(&parser.current, message); }

static void errorAtCurrent(const char* message) {}

static Token advance() {
    parser.previous = parser.current;
    // the while loop helps
    // skip the error tokens
    // that are produced by the lexer
    while (true) {
        parser.current = scanToken();
        // break if the current token is not error
        // if it is an error then report the error
        // and eat following error tokens if any
        if (parser.current.type != TOKEN_ERROR) break;
        errorAtCurrent(parser.current.start);
    }
}

static void consume(TokenType type, const char* message) {
    if (parser.current.type == type) {
        advance();
        return;
    }
    errorAtCurrent(message);
}

static Chunk* currentChunk() { return compilingChunk; }

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitReturn() { emitByte(OP_RETURN); }

static uint8_t makeConstant(Value value) {
    // addConstant returns the index in the pool to which
    // the constant was added.
    int constantIndex = addConstant(currentChunk(), value);
    if (constantIndex > UINT8_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }
    return (uint8_t)constantIndex;
}

static void emitConstant(Value value) {
    // first add the op_constant opcode
    // then the index in the constant pool
    // as it's operand (returned by makeConstant)
    emitBytes(OP_CONSTANT, makeConstant(value));
}

static void endCompiler() {
    emitReturn();
#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(), "code");
    }
#endif
}

// actual parsing functions and some forward declarations

static void binary();
static void unary();
static void literal();
static void number();
static void expression();
static void grouping();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);

ParseRule rules[] = {
    {grouping, NULL, PREC_NONE},  // TOKEN_LEFT_PAREN
    {NULL, NULL, PREC_NONE},      // TOKEN_RIGHT_PAREN
    {NULL, NULL, PREC_NONE},      // TOKEN_LEFT_BRACE
    {NULL, NULL, PREC_NONE},      // TOKEN_RIGHT_BRACE
    {NULL, NULL, PREC_NONE},      // TOKEN_COMMA
    {NULL, NULL, PREC_NONE},      // TOKEN_DOT
    {unary, binary, PREC_TERM},   // TOKEN_MINUS
    {NULL, binary, PREC_TERM},    // TOKEN_PLUS
    {NULL, NULL, PREC_NONE},      // TOKEN_SEMICOLON
    {NULL, binary, PREC_FACTOR},  // TOKEN_SLASH
    {NULL, binary, PREC_FACTOR},  // TOKEN_STAR
    {NULL, NULL, PREC_NONE},      // TOKEN_BANG
    {NULL, NULL, PREC_NONE},      // TOKEN_BANG_EQUAL
    {NULL, NULL, PREC_NONE},      // TOKEN_EQUAL
    {NULL, NULL, PREC_NONE},      // TOKEN_EQUAL_EQUAL
    {NULL, NULL, PREC_NONE},      // TOKEN_GREATER
    {NULL, NULL, PREC_NONE},      // TOKEN_GREATER_EQUAL
    {NULL, NULL, PREC_NONE},      // TOKEN_LESS
    {NULL, NULL, PREC_NONE},      // TOKEN_LESS_EQUAL
    {NULL, NULL, PREC_NONE},      // TOKEN_IDENTIFIER
    {NULL, NULL, PREC_NONE},      // TOKEN_STRING
    {number, NULL, PREC_NONE},    // TOKEN_NUMBER
    {NULL, NULL, PREC_NONE},      // TOKEN_AND
    {NULL, NULL, PREC_NONE},      // TOKEN_CLASS
    {NULL, NULL, PREC_NONE},      // TOKEN_ELSE
    {literal, NULL, PREC_NONE},   // TOKEN_FALSE
    {NULL, NULL, PREC_NONE},      // TOKEN_FOR
    {NULL, NULL, PREC_NONE},      // TOKEN_FUN
    {NULL, NULL, PREC_NONE},      // TOKEN_IF
    {literal, NULL, PREC_NONE},   // TOKEN_NIL
    {NULL, NULL, PREC_NONE},      // TOKEN_OR
    {NULL, NULL, PREC_NONE},      // TOKEN_PRINT
    {NULL, NULL, PREC_NONE},      // TOKEN_RETURN
    {NULL, NULL, PREC_NONE},      // TOKEN_SUPER
    {NULL, NULL, PREC_NONE},      // TOKEN_THIS
    {literal, NULL, PREC_NONE},   // TOKEN_TRUE
    {NULL, NULL, PREC_NONE},      // TOKEN_VAR
    {NULL, NULL, PREC_NONE},      // TOKEN_WHILE
    {NULL, NULL, PREC_NONE},      // TOKEN_ERROR
    {NULL, NULL, PREC_NONE},      // TOKEN_EOF
};

static ParseRule* getRule(TokenType type) {
    // wonder why it returns a pointer to the
    // the rule instead of return rules[type]
    return &rules[type];
}

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;

    if (prefixRule == NULL) {
        error("Expected expression");
        return;
    }

    prefixRule();

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule();
    }
}

static void number() {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void literal() {
    switch (parser.previous.type) {
        case TOKEN_TRUE:
            emitByte(OP_TRUE);
            break;
        case TOKEN_FALSE:
            emitByte(OP_FALSE);
            break;
        case TOKEN_NIL:
            emitByte(OP_NIL);
            break;
        default:
            return;  // Unreachable
    }
}

static void expression() { parsePrecedence(PREC_ASSIGN); }

static void binary() {
    TokenType operator= parser.previous.type;

    //  Compile the right hand operand
    ParseRule* rule = getRule(operator);
    //  + 1 because binary operators like +, -, %, *
    //  assosciate to the left , so 1 + 2 + 3
    //  should be parsed as (1 + 2) + 3 instead of  1 + (2 + 3)
    //  Each binary operator’s right-hand operand precedence is one level higher
    //  than its own.
    parsePrecedence((Precedence)(rule->precedence + 1));

    //  Emit the operator instruction
    switch (operator) {
        case TOKEN_PLUS:
            emitByte(OP_ADD);
            break;
        case TOKEN_MINUS:
            emitByte(OP_SUB);
            break;
        case TOKEN_STAR:
            emitByte(OP_MULT);
            break;
        case TOKEN_SLASH:
            emitByte(OP_DIV);
            break;
        default:
            return;  // Unreachable.
    }
}

static void unary() {
    TokenType operatorType = parser.previous.type;
    // compile the operand
    parsePrecedence(PREC_UNARY);
    // emit the operator instruction.
    switch (operatorType) {
        case TOKEN_MINUS:
            emitByte(OP_NEGATE);
            break;
        default:
            return;  // Unreachable
    }
}

// assume the initial '(' is already consumed
static void grouping() {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.");
}

bool compile(const char* source, Chunk* chunk) {
    initScanner(source);
    compilingChunk = chunk;
    // initially both parser.current and parser.previous
    // are set to nothing. calling it for the first time
    // sets the current token to the first token in the
    // source file, and sets the previous token to well.. nothing
    advance();
    // since the current token is now set to the first token
    // in source, calling advance() now will set parser.previous
    // to the first token in source and we can go from there.

    // in clox, there is no buffer of tokens, so we just use
    // parser.current instead of peek() to lookahead
    expression();
    consume(TOKEN_EOF, "Expected end of expression.");
    endCompiler();
    return !parser.hadError;
}
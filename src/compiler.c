#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

#include "debug.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

typedef struct {
    Token current;
    Token previous;
    bool hadError;
    bool panicMode;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_ASSIGNMENT,  // = += -= *= /= %= **=
    PREC_OR,          // or
    PREC_AND,         // and
    PREC_EQUALITY,    // == !=
    PREC_COMPARISON,  // < > <= >=
    PREC_TERM,        // + -
    PREC_FACTOR,      // * / %
    PREC_UNARY,       // ! -
    PREC_POWER,       // **
    PREC_CALL,        // ()
    PREC_SUBSCRIPT,   // [] .
    PREC_PRIMARY
} Precedence;

typedef void (*ParseFn)(bool canAssign);

typedef struct {
    ParseFn prefix;
    ParseFn infix;
    Precedence precedence;
} ParseRule;

typedef struct {
    Token name;
    int depth;
    bool isCaptured;
} Local;

typedef struct {
    uint8_t index;  // TODO change this
    bool isLocal;
} Upvalue;

typedef enum {
    TYPE_FUNCTION,
    TYPE_SCRIPT
} FunctionType;

// TODO decide on operand/wide semantics
// Only 256 upvalues but way more locals etc.
typedef struct Compiler {
    struct Compiler* enclosing;
    ObjFunction* function;
    FunctionType type;

    Local* locals;  // TODO verify locals are properly garbage collected
    int localCount;
    int localCapacity;
    Upvalue upvalues[UINT8_COUNT];
    int scopeDepth;
} Compiler;

Parser parser;

Compiler* current = NULL;

// Types and globals for continue/break statements
typedef struct BreakJump {
    int scopeDepth;
    int offset;
    struct BreakJump* next;
} BreakJump;

int innermostLoopStart = -1;
int innermostLoopScopeDepth = 0;

BreakJump* breakJumps = NULL;

// Forward declarations to get around recursive nature of grammar
static void expression();
static void statement();
static void declaration();
static ParseRule* getRule(TokenType type);
static void parsePrecedence(Precedence precedence);
static uint16_t identifierConstant(Token* name);
static uint16_t parseVariable(const char* errorMessage);
static void defineVariable(uint16_t global);
static int resolveLocal(Compiler* compiler, Token* name);
static void markInitialized();
static uint8_t argumentList();
static int resolveUpvalue(Compiler* compiler, Token* name);

static void writeLocal(Compiler* compiler, Local* local) {
    if (compiler->localCapacity < compiler->localCount + 1) {
        int oldCapacity = compiler->localCapacity;
        compiler->localCapacity = GROW_CAPACITY(oldCapacity);
        compiler->locals = GROW_ARRAY(compiler->locals, Local, oldCapacity, compiler->localCapacity);
    }

    compiler->locals[compiler->localCount] = *local;
    compiler->localCount++;
}

static Chunk* currentChunk() {
    return &current->function->chunk;
}

static void errorAt(Token* token, const char* message) {
    if (parser.panicMode) return;
    parser.panicMode = true;

    fprintf(stderr, "[line %d] Error", token->line);

    if (token->type == TOKEN_EOF) {
        fprintf(stderr, " at end");
    } else if (token->type == TOKEN_ERROR) {
        // Nothing.
    } else {
        fprintf(stderr, " at '%.*s'", token->length, token->start);
    }

    fprintf(stderr, ": %s\n", message);
    parser.hadError = true;
}

static void error(const char* message) {
    errorAt(&parser.previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(&parser.current, message);
}

static void advance() {
    parser.previous = parser.current;

    for (;;) {
        parser.current = scanToken();
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

static bool check(TokenType type) {
    return parser.current.type == type;
}

static bool match(TokenType type) {
    if (!check(type)) return false;
    advance();
    return true;
}

static void emitByte(uint8_t byte) {
    writeChunk(currentChunk(), byte, parser.previous.line);
}

static void emitBytes(uint8_t byte1, uint8_t byte2) {
    emitByte(byte1);
    emitByte(byte2);
}

static void emitLoop(int loopStart) {
    emitByte(OP_LOOP);

    int offset = currentChunk()->count - loopStart + 2;
    if (offset > UINT16_MAX) error("Loop body too large.");

    emitByte((offset >> 8) & 0xff);
    emitByte(offset & 0xff);
}

static int emitJump(uint8_t intstruction) {
    emitByte(intstruction);
    emitByte(0xff);
    emitByte(0xff);
    return currentChunk()->count - 2;
}

static void emitReturn() {
    emitByte(OP_NIL);
    emitByte(OP_RETURN);
}

static uint16_t makeConstant(Value value) {
    int constant = addConstant(currentChunk(), value);
    if (constant > UINT16_MAX) {
        error("Too many constants in one chunk.");
        return 0;
    }

    return (uint16_t)constant;
}

static void emitConstant(Value value) {
    uint16_t constant = makeConstant(value);
    if (constant < 256) {
        emitBytes(OP_CONSTANT, (uint8_t)constant);
    } else {
        emitByte(OP_WIDE);
        emitByte(OP_CONSTANT);
        emitByte((uint8_t)(constant >> 8));
        emitByte((uint8_t)constant);
    }
}

static void patchJump(int offset) {
    // -2 to adjust for the bytecode for the jump offset itself
    int jump = currentChunk()->count - offset - 2;

    if (jump > UINT16_MAX) {
        error("Too much code to jump over.");
    }

    currentChunk()->code[offset] = (jump >> 8) & 0xff;
    currentChunk()->code[offset + 1] = jump & 0xff;
}

static void initCompiler(Compiler* compiler, FunctionType type) {
    compiler->enclosing = current;
    compiler->function = NULL;
    compiler->type = type;
    compiler->locals = NULL;
    compiler->localCount = 0;
    compiler->localCapacity = 0;
    compiler->scopeDepth = 0;
    compiler->function = newFunction();
    current = compiler;

    if (type != TYPE_SCRIPT) {
        current->function->name = copyString(parser.previous.start,
                                             parser.previous.length);
    }

    Local local;
    local.depth = 0;
    local.isCaptured = false;
    local.name.start = "";
    local.name.length = 0;
    writeLocal(current, &local);
}

static ObjFunction* endCompiler() {
    emitReturn();
    ObjFunction* function = current->function;

#ifdef DEBUG_PRINT_CODE
    if (!parser.hadError) {
        disassembleChunk(currentChunk(),
            function->name != NULL ? function->name->chars : "<script>");
    }
#endif

    current = current->enclosing;
    return function;
}

static void beginScope() {
    current->scopeDepth++;
}

static void emitPops(uint16_t popCount) {
    if (popCount == 1) {
        emitByte(OP_POP);
        return;
    }

    while (popCount > 0) {
        int count = popCount >= 255 ? 255 : popCount;
        emitByte(OP_POP_N);
        emitByte(count);
        popCount = popCount - count;
    }
}

static void endScope() {
    current->scopeDepth--;

    uint16_t popCount = 0;
    while (current->localCount > 0 &&
        current->locals[current->localCount - 1].depth > current->scopeDepth) {
            if (current->locals[current->localCount - 1].isCaptured) {
                // Emit any stored up pops first
                emitPops(popCount);
                popCount = 0;
                emitByte(OP_CLOSE_UPVALUE);
            } else {
                popCount++;
            }
            current->localCount--;
        }

    // Emit any leftover pops
    emitPops(popCount);
}

static void patchBreakJumps() {
    while (breakJumps != NULL) {
        if (breakJumps->scopeDepth >= innermostLoopScopeDepth) {
            // Patch break jump
            patchJump(breakJumps->offset);

            // Remove node from linked list
            BreakJump* temp = breakJumps;
            breakJumps = breakJumps->next;
            FREE(BreakJump, temp);
        } else {
            break;
        }
    }
}

static void binary(bool canAssign) {
    // Remember the operator.
    TokenType operatorType = parser.previous.type;

    // Compile the right operand.
    ParseRule* rule = getRule(operatorType);
    parsePrecedence((Precedence)(rule->precedence + 1));

    // Emit the operator instruction.
    switch (operatorType) {
    case TOKEN_BANG_EQUAL:    emitBytes(OP_EQUAL, OP_NOT); break;
    case TOKEN_EQUAL_EQUAL:   emitByte(OP_EQUAL); break;
    case TOKEN_GREATER:       emitByte(OP_GREATER); break;
    case TOKEN_GREATER_EQUAL: emitBytes(OP_LESS, OP_NOT); break;
    case TOKEN_LESS:          emitByte(OP_LESS); break;
    case TOKEN_LESS_EQUAL:    emitBytes(OP_GREATER, OP_NOT); break;
    case TOKEN_PLUS:          emitByte(OP_ADD); break;
    case TOKEN_MINUS:         emitByte(OP_SUBTRACT); break;
    case TOKEN_STAR:          emitByte(OP_MULTIPLY); break;
    case TOKEN_SLASH:         emitByte(OP_DIVIDE); break;
    case TOKEN_PERCENT:       emitByte(OP_MODULO); break;
    case TOKEN_STAR_STAR:     emitByte(OP_POWER); break;
    default:
        return; // Unreachable.
    }
}

static void call(bool canAssign) {
    uint8_t argCount = argumentList();
    emitBytes(OP_CALL, argCount);
}

static void subscript(bool canAssign) {
    parsePrecedence(PREC_OR);
    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after index.");

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitByte(OP_STORE_SUBSCR);
    } else {
        emitByte(OP_INDEX_SUBSCR);
    }
}

static void dot(bool canAssign) {
    consume(TOKEN_IDENTIFIER, "Expect identifier after '.'.");
    emitConstant(OBJ_VAL(copyString(parser.previous.start, parser.previous.length)));

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        emitByte(OP_STORE_SUBSCR);
    } else {
        emitByte(OP_INDEX_SUBSCR);
    }
}

static void literal(bool canAssign) {
    switch (parser.previous.type) {
        case TOKEN_FALSE: emitByte(OP_FALSE); break;
        case TOKEN_NIL: emitByte(OP_NIL); break;
        case TOKEN_TRUE: emitByte(OP_TRUE); break;
        default:
            return; // Unreachable
    }
}

static void expression() {
    parsePrecedence(PREC_ASSIGNMENT);
}

static void block() {
    while (!check(TOKEN_RIGHT_BRACE) && !check(TOKEN_EOF)) {
        declaration();
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after block.");
}

static void function(FunctionType type) {
    Compiler compiler;
    initCompiler(&compiler, type);
    beginScope();

    // Compile the paramter list
    consume(TOKEN_LEFT_PAREN, "Expect '(' after function name.");
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            current->function->arity++;
            if (current->function->arity > 255) {
                errorAtCurrent("Cannot have more than 255 parameters.");
            }

            uint8_t paramConstant = parseVariable("Expect parameter name.");
            defineVariable(paramConstant);
        } while (match(TOKEN_COMMA));
    }
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after parameters.");

    // The body
    consume(TOKEN_LEFT_BRACE, "Expect '{' before function body.");
    block();

    // Create the function object
    ObjFunction* function = endCompiler();
    emitBytes(OP_CLOSURE, makeConstant(OBJ_VAL(function)));

    for (int i = 0; i < function->upvalueCount; i++) {
        emitByte(compiler.upvalues[i].isLocal ? 1 : 0);
        emitByte(compiler.upvalues[i].index);
    }
    // TODO handle 2 byte operand
    // uint16_t constant = makeConstant(OBJ_VAL(function));
    // if (constant < 256) {
    //     emitByte(OP_CONSTANT);
    //     emitByte(constant & 0xff);
    // } else {
    //     emitByte(OP_WIDE);
    //     emitByte(OP_CONSTANT);
    //     emitByte((constant >> 8) & 0xff);
    //     emitByte(constant & 0xff);
    // }
}

static void funDeclaration() {
    uint8_t global = parseVariable("Expect function name.");
    markInitialized();
    function(TYPE_FUNCTION);
    defineVariable(global);
}

static void varDeclaration() {
    uint16_t global = parseVariable("Expect variable name.");

    if (match(TOKEN_EQUAL)) {
        expression();
    } else {
        emitByte(OP_NIL);
    }
    consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");

    defineVariable(global);
}

static void breakStatement() {
    if (innermostLoopStart == -1) {
        error("Cannot use 'break' outside of a loop.");
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after 'break'.");

    // Discard any locals created inside the loop.
    for (int i = current->localCount - 1;
        i >= 0 && current->locals[i].depth > innermostLoopScopeDepth;
        i--) {
        // TODO optimize with OP_POP_N
        emitByte(OP_POP);
    }

    // Jump to the end of the loop
    // This needs to be patched when loop block is exited
    int jmp = emitJump(OP_JUMP);

    // Add breakJump to start of linked list
    BreakJump* breakJump = ALLOCATE(BreakJump, 1);
    breakJump->scopeDepth = innermostLoopScopeDepth;
    breakJump->offset = jmp;
    breakJump->next = breakJumps;
    breakJumps = breakJump;
}

static void continueStatement() {
    if (innermostLoopStart == -1) {
        error("Cannot use 'continue' outside of a loop.");
    }

    consume(TOKEN_SEMICOLON, "Expect ';' after 'continue'.");

    // Discard any locals created inside the loop.
    for (int i = current->localCount - 1;
        i >= 0 && current->locals[i].depth > innermostLoopScopeDepth;
        i--) {
        // TODO optimize with OP_POP_N
        emitByte(OP_POP);
    }

    // Jump to top of current innermost loop.
    emitLoop(innermostLoopStart);
}

static void expressionStatement() {
    expression();
    consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
    emitByte(OP_POP);
}

static void forStatement() {
    beginScope();
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'for'.");
    if (match(TOKEN_SEMICOLON)) {
        // Not initializer
    } else if (match(TOKEN_LET)) {
        varDeclaration();
    } else {
        expressionStatement();
    }

    int surroundingLoopStart = innermostLoopStart;
    int surroundingLoopScopeDepth = innermostLoopScopeDepth;
    innermostLoopStart = currentChunk()->count;
    innermostLoopScopeDepth = current->scopeDepth;

    int exitJump = -1;
    if (!match(TOKEN_SEMICOLON)) {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after loop condition.");

        // Jump out of the loop if the condition is false
        exitJump = emitJump(OP_JUMP_IF_FALSE);
        emitByte(OP_POP); // Condition
    }

    if (!match(TOKEN_RIGHT_PAREN)) {
        int bodyJump = emitJump(OP_JUMP);

        int incrementStart = currentChunk()->count;
        expression();
        emitByte(OP_POP);
        consume(TOKEN_RIGHT_PAREN, "Expect a ')' after for clauses.");

        emitLoop(innermostLoopStart);
        innermostLoopStart = incrementStart;
        patchJump(bodyJump);
    }

    statement();

    emitLoop(innermostLoopStart);

    if (exitJump != -1) {
        patchJump(exitJump);
        emitByte(OP_POP); // Condition
    }

    patchBreakJumps();

    innermostLoopStart = surroundingLoopStart;
    innermostLoopScopeDepth = surroundingLoopScopeDepth;

    endScope();
}

static void ifStatement() {
    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'if'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int thenJump = emitJump(OP_JUMP_IF_FALSE);
    emitByte(OP_POP);
    statement();

    int elseJump = emitJump(OP_JUMP);

    patchJump(thenJump);
    emitByte(OP_POP);

    if (match(TOKEN_ELSE)) statement();
    patchJump(elseJump);
}

static void returnStatement() {
    if (current->type == TYPE_SCRIPT) {
        error("Cannot return from top-level code.");
    }

    if (match(TOKEN_SEMICOLON)) {
        emitReturn();
    } else {
        expression();
        consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
        emitByte(OP_RETURN);
    }
}

static void whileStatement() {
    int surroundingLoopStart = innermostLoopStart;
    int surroundingLoopScopeDepth = innermostLoopScopeDepth;
    innermostLoopStart = currentChunk()->count;
    innermostLoopScopeDepth = current->scopeDepth;

    consume(TOKEN_LEFT_PAREN, "Expect '(' after 'while'.");
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after condition.");

    int exitJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    statement();

    emitLoop(innermostLoopStart);

    patchJump(exitJump);
    emitByte(OP_POP);

    patchBreakJumps();

    innermostLoopStart = surroundingLoopStart;
    innermostLoopScopeDepth = surroundingLoopScopeDepth;
}

static void synchronize() {
    parser.panicMode = false;

    while (parser.current.type != TOKEN_EOF) {
        if (parser.previous.type == TOKEN_SEMICOLON) return;

        switch (parser.current.type) {
        case TOKEN_FUN:
        case TOKEN_LET:
        case TOKEN_FOR:
        case TOKEN_IF:
        case TOKEN_WHILE:
        case TOKEN_RETURN:
            return;

        default:
            // Do nothing.
            ;
        }

        advance();
    }
}

static void declaration() {
    if (match(TOKEN_FUN)) {
        funDeclaration();
    } else if (match(TOKEN_LET)) {
        varDeclaration();
    } else {
        statement();
    }

    if (parser.panicMode) synchronize();
}

static void statement() {
    if (match(TOKEN_BREAK)) {
        breakStatement();
    } else if (match(TOKEN_CONTINUE)) {
        continueStatement();
    } else if (match(TOKEN_FOR)) {
        forStatement();
    } else if (match(TOKEN_IF)) {
        ifStatement();
    } else if (match(TOKEN_RETURN)) {
        returnStatement();
    } else if (match(TOKEN_WHILE)) {
        whileStatement();
    } else if (match(TOKEN_LEFT_BRACE)) {
        beginScope();
        block();
        endScope();
    } else {
        expressionStatement();
    }
}

static void grouping(bool canAssign) {
    expression();
    consume(TOKEN_RIGHT_PAREN, "Expect ')' after expression.");
}

static void number(bool canAssign) {
    double value = strtod(parser.previous.start, NULL);
    emitConstant(NUMBER_VAL(value));
}

static void and_(bool canAssign) {
    int endJump = emitJump(OP_JUMP_IF_FALSE);

    emitByte(OP_POP);
    parsePrecedence(PREC_AND);

    patchJump(endJump);
}

static void or_(bool canAssign) {
    int elseJump = emitJump(OP_JUMP_IF_FALSE);
    int endJump = emitJump(OP_JUMP);

    patchJump(elseJump);
    emitByte(OP_POP);

    parsePrecedence(PREC_OR);
    patchJump(endJump);
}

static void basicString(bool canAssign) {
    // Basic strings can have escape sequences.
    // Need to scan token chars and build up new string with converted
    // escape sequences.
    int tokenLen = parser.previous.length - 2;
    int realLen = 0;

    char* new = ALLOCATE(char, tokenLen);

    for (int i = 1; i < tokenLen + 1; ++i) {
        char c = parser.previous.start[i];

        // Can't have escape sequence with only 1 char left
        if (i < tokenLen && c == '\\') {
            char next = parser.previous.start[++i];
            switch (next) {
                case '\n':
                    break;
                case '\\':
                    new[realLen++] = '\\';
                    break;
                case '\'':
                    new[realLen++] = '\'';
                    break;
                case '\"':
                    new[realLen++] = '\"';
                    break;
                case 'n':
                    new[realLen++] = '\n';
                    break;
                case 't':
                    new[realLen++] = '\t';
                    break;
                default:
                    break;
            }
            continue;
        }

        new[realLen++] = c;
    }
    emitConstant(OBJ_VAL(copyString(new, realLen)));
    FREE(char, new);
}

// TODO support actual templating
static void templateString(bool canAssign) {
    // Template strings can have escape sequences.
    // Need to scan token chars and build up new string with converted
    // escape sequences.
    int tokenLen = parser.previous.length - 2;
    int realLen = 0;

    char* new = ALLOCATE(char, tokenLen);

    for (int i = 1; i < tokenLen + 1; ++i) {
        char c = parser.previous.start[i];

        // Can't have escape sequence with only 1 char left
        if (i < tokenLen && c == '\\') {
            char next = parser.previous.start[++i];
            switch (next) {
                case '\n':
                    break;
                case '\\':
                    new[realLen++] = '\\';
                    break;
                case '\'':
                    new[realLen++] = '\'';
                    break;
                case '\"':
                    new[realLen++] = '\"';
                    break;
                case 'n':
                    new[realLen++] = '\n';
                    break;
                case 't':
                    new[realLen++] = '\t';
                    break;
                default:
                    break;
            }
            continue;
        }

        new[realLen++] = c;
    }
    emitConstant(OBJ_VAL(copyString(new, realLen)));
    FREE(char, new);
}

static void rawString(bool canAssign) {
      emitConstant(OBJ_VAL(copyString(parser.previous.start + 1, parser.previous.length - 2)));
}

static void list(bool canAssign) {
    int itemCount = 0;
    if (!check(TOKEN_RIGHT_BRACKET)) {
        do {
            if (check(TOKEN_RIGHT_BRACKET)) {
                // Trailing comma case
                break;
            }

            parsePrecedence(PREC_OR);

            if (itemCount == UINT16_COUNT) {
                error("Cannot have more than 65536 items in a list display.");
            }
            itemCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACKET, "Expect ']' after list elements.");

    if (itemCount < 256) {
        emitByte(OP_BUILD_LIST);
        emitByte(itemCount);
    } else {
        emitBytes(OP_WIDE, OP_BUILD_LIST);
        emitByte((uint8_t)(itemCount >> 8));
        emitByte((uint8_t)itemCount);
    }

    return;
}

// Note that if a map is defined in a position where it could be interpreted as
// something like an expression statement, then it will be parse as if it were a
// block. https://stackoverflow.com/questions/8089737/javascript-object-parsing
static void map(bool canAssign) {
    int itemCount = 0;
    if (!check(TOKEN_RIGHT_BRACE)) {
        do {
            if (check(TOKEN_RIGHT_BRACE)) {
                // Trailing comma case
                break;
            }

            parsePrecedence(PREC_OR);
            consume(TOKEN_COLON, "Expect ':' between key and value pair of map.");
            parsePrecedence(PREC_OR);

            if (itemCount == UINT16_COUNT) {
                error("Cannot have more than 65536 items in a map display.");
            }
            itemCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_BRACE, "Expect '}' after map elements.");

    if (itemCount < 256) {
        emitByte(OP_BUILD_MAP);
        emitByte(itemCount);
    } else {
        emitBytes(OP_WIDE, OP_BUILD_MAP);
        emitByte((uint8_t)(itemCount >> 8));
        emitByte((uint8_t)itemCount);
    }

    return;
}

static void namedVariable(Token name, bool canAssign) {
#define SHORT_HAND_ASSIGNER(op) \
    do { \
        if (arg < 256) { \
            emitBytes(getOp, (uint8_t)arg); \
            expression(); \
            emitByte(op); \
            emitBytes(setOp, (uint8_t)arg); \
        } else { \
            emitByte(OP_WIDE); \
            emitByte(getOp); \
            emitByte((uint8_t)(arg >> 8)); \
            emitByte((uint8_t)arg); \
            expression(); \
            emitByte(op); \
            emitByte(OP_WIDE); \
            emitByte(setOp); \
            emitByte((uint8_t)(arg >> 8)); \
            emitByte((uint8_t)arg); \
        } \
    } while (false)

    uint8_t getOp, setOp;
    int arg = resolveLocal(current, &name);
    if (arg != -1) {
        getOp = OP_GET_LOCAL;
        setOp = OP_SET_LOCAL;
    } else if ((arg = resolveUpvalue(current, &name)) != -1) {
        getOp = OP_GET_UPVALUE;
        setOp = OP_SET_UPVALUE;
    } else { 
        arg = identifierConstant(&name);
        getOp = OP_GET_GLOBAL;
        setOp = OP_SET_GLOBAL;
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        expression();
        if (arg < 256) {
            emitBytes(setOp, (uint8_t)arg);
        } else {
            emitByte(OP_WIDE);
            emitByte(setOp);
            emitByte((uint8_t)(arg >> 8));
            emitByte((uint8_t)arg);
        }
    } else if (canAssign && match(TOKEN_PLUS_EQUAL)) {
        SHORT_HAND_ASSIGNER(OP_ADD);
    } else if (canAssign && match(TOKEN_MINUS_EQUAL)) {
        SHORT_HAND_ASSIGNER(OP_SUBTRACT);
    } else if (canAssign && match(TOKEN_STAR_EQUAL)) {
        SHORT_HAND_ASSIGNER(OP_MULTIPLY);
    } else if (canAssign && match(TOKEN_SLASH_EQUAL)) {
        SHORT_HAND_ASSIGNER(OP_DIVIDE);
    } else if (canAssign && match(TOKEN_PERCENT_EQUAL)) {
        SHORT_HAND_ASSIGNER(OP_MODULO);
    } else if (canAssign && match(TOKEN_STAR_STAR_EQUAL)) {
        SHORT_HAND_ASSIGNER(OP_POWER);
    } else {
        if (arg < 256) {
            emitBytes(getOp, (uint8_t)arg);
        } else {
            emitByte(OP_WIDE);
            emitByte(getOp);
            emitByte((uint8_t)(arg >> 8));
            emitByte((uint8_t)arg);
        }
    }
#undef SHORT_HAND_ASSIGNER
}

static void variable(bool canAssign) {
    namedVariable(parser.previous, canAssign);
}

static void unary(bool canAssign) {
    TokenType operatorType = parser.previous.type;

    // Compile the operand
    parsePrecedence(PREC_UNARY);

    // Emit the operator instruction
    switch (operatorType) {
        case TOKEN_BANG: emitByte(OP_NOT); break;
        case TOKEN_MINUS: emitByte(OP_NEGATE); break;
        default:
            return; // Unreachable
    }
}

ParseRule rules[] = { 
    // Prefix          Infix       Precedence
    { grouping,        call,       PREC_CALL },        // TOKEN_LEFT_PAREN
    { NULL,            NULL,       PREC_NONE },        // TOKEN_RIGHT_PAREN
    { map,             NULL,       PREC_NONE },        // TOKEN_LEFT_BRACE
    { NULL,            NULL,       PREC_NONE },        // TOKEN_RIGHT_BRACE
    { list,            subscript,  PREC_SUBSCRIPT },   // TOKEN_LEFT_BRACKET
    { NULL,            NULL,       PREC_NONE },        // TOKEN_RIGHT_BRACKET
    { NULL,            NULL,       PREC_NONE },        // TOKEN_COMMA
    { NULL,            dot,        PREC_SUBSCRIPT },   // TOKEN_DOT
    { unary,           binary,     PREC_TERM },        // TOKEN_MINUS
    { NULL,            binary,     PREC_TERM },        // TOKEN_PLUS
    { NULL,            NULL,       PREC_NONE },        // TOKEN_SEMICOLON
    { NULL,            NULL,       PREC_NONE },        // TOKEN_COLON
    { NULL,            binary,     PREC_FACTOR },      // TOKEN_SLASH
    { NULL,            binary,     PREC_FACTOR },      // TOKEN_PERCENT
    { NULL,            binary,     PREC_FACTOR },      // TOKEN_STAR
    { NULL,            binary,     PREC_POWER },       // TOKEN_STAR_STAR
    { unary,           NULL,       PREC_NONE },        // TOKEN_BANG
    { NULL,            binary,     PREC_EQUALITY },    // TOKEN_BANG_EQUAL
    { NULL,            NULL,       PREC_NONE },        // TOKEN_EQUAL
    { NULL,            binary,     PREC_EQUALITY },    // TOKEN_EQUAL_EQUAL
    { NULL,            binary,     PREC_COMPARISON },  // TOKEN_GREATER
    { NULL,            binary,     PREC_COMPARISON },  // TOKEN_GREATER_EQUAL
    { NULL,            binary,     PREC_COMPARISON },  // TOKEN_LESS
    { NULL,            binary,     PREC_COMPARISON },  // TOKEN_LESS_EQUAL
    { NULL,            binary,     PREC_NONE },        // TOKEN_PLUS_EQUAL
    { NULL,            binary,     PREC_NONE },        // TOKEN_MINUS_EQUAL
    { NULL,            binary,     PREC_NONE },        // TOKEN_STAR_EQUAL
    { NULL,            binary,     PREC_NONE },        // TOKEN_SLASH_EQUAL
    { NULL,            binary,     PREC_NONE },        // TOKEN_PERCENT_EQUAL
    { NULL,            binary,     PREC_NONE },        // TOKEN_STAR_STAR_EQUAL
    { basicString,     NULL,       PREC_NONE },        // TOKEN_BASIC_STRING
    { variable,        NULL,       PREC_NONE },        // TOKEN_IDENTIFIER
    { number,          NULL,       PREC_NONE },        // TOKEN_NUMBER
    { templateString,  NULL,       PREC_NONE },        // TOKEN_TEMPLATE_STRING
    { rawString,       NULL,       PREC_NONE },        // TOKEN_RAW_STRING
    { NULL,            and_,       PREC_AND },         // TOKEN_AND
    { NULL,            NULL,       PREC_NONE },        // TOKEN_BREAK
    { NULL,            NULL,       PREC_NONE },        // TOKEN_CONTINUE
    { NULL,            NULL,       PREC_NONE },        // TOKEN_ELSE
    { literal,         NULL,       PREC_NONE },        // TOKEN_FALSE
    { NULL,            NULL,       PREC_NONE },        // TOKEN_FOR
    { NULL,            NULL,       PREC_NONE },        // TOKEN_FUN
    { NULL,            NULL,       PREC_NONE },        // TOKEN_IF
    { NULL,            NULL,       PREC_NONE },        // TOKEN_LET
    { literal,         NULL,       PREC_NONE },        // TOKEN_NIL
    { NULL,            or_,        PREC_OR },          // TOKEN_OR
    { NULL,            NULL,       PREC_NONE },        // TOKEN_RETURN
    { literal,         NULL,       PREC_NONE },        // TOKEN_TRUE
    { NULL,            NULL,       PREC_NONE },        // TOKEN_WHILE
    { NULL,            NULL,       PREC_NONE },        // TOKEN_ERROR
    { NULL,            NULL,       PREC_NONE },        // TOKEN_EOF
};

static void parsePrecedence(Precedence precedence) {
    advance();
    ParseFn prefixRule = getRule(parser.previous.type)->prefix;
    if (prefixRule == NULL) {
        error("Expect expression.");
        return;
    }

    bool canAssign = precedence <= PREC_ASSIGNMENT;
    prefixRule(canAssign);

    while (precedence <= getRule(parser.current.type)->precedence) {
        advance();
        ParseFn infixRule = getRule(parser.previous.type)->infix;
        infixRule(canAssign);
    }

    if (canAssign && match(TOKEN_EQUAL)) {
        error("Invalid assignment target.");
    }
}

static uint16_t identifierConstant(Token* name) {
    return makeConstant(OBJ_VAL(copyString(name->start, name->length)));
}

static bool identifiersEqual(Token* a, Token* b) {
    if (a->length != b->length) return false;
    return memcmp(a->start, b->start, a->length) == 0;
}

static int resolveLocal(Compiler* compiler, Token* name) {
    for (int i = compiler->localCount - 1; i >= 0; i--) {
        Local* local = &compiler->locals[i];
        if (identifiersEqual(name, &local->name)) {
            if (local->depth == -1) {
                error("Cannot read local variable in its own initializer.");
            }
            return i;
        }
    }
    
    return -1;
}

static int addUpvalue(Compiler* compiler, uint8_t index, bool isLocal) {
    int upvalueCount = compiler->function->upvalueCount;

    for (int i = 0; i < upvalueCount; i++) {
        Upvalue* upvalue = &compiler->upvalues[i];
        if (upvalue->index == index && upvalue->isLocal == isLocal) {
            return i;
        }
    }

    if (upvalueCount == UINT8_COUNT) {
        error("Too many closure variables in function.");
        return 0;
    }

    compiler->upvalues[upvalueCount].isLocal = isLocal;
    compiler->upvalues[upvalueCount].index = index;
    return compiler->function->upvalueCount++;
}

static int resolveUpvalue(Compiler* compiler, Token* name) {
    if (compiler->enclosing == NULL) return -1;

    int local = resolveLocal(compiler->enclosing, name);
    if (local != -1) {
        compiler->enclosing->locals[local].isCaptured = true;
        return addUpvalue(compiler, (uint8_t)local, true);
    }

    int upvalue = resolveUpvalue(compiler->enclosing, name);
    if (upvalue != -1) {
        return addUpvalue(compiler, (uint8_t)upvalue, false);
    }

    return -1;
}

static void addLocal(Token name) {
    if (current->localCount == UINT16_COUNT) {
        error("Too many local variables in function.");
        return;
    }
    Local local;
    local.name = name;
    local.depth = -1;
    local.isCaptured = false;
    writeLocal(current, &local);
}

static void declareVariable() {
    // Global variables are implicitly declared.
    if (current->scopeDepth == 0) return;

    Token* name = &(parser.previous);
    for (int i = current->localCount - 1; i >= 0; i--) {
        Local* local = &current->locals[i];
        if (local->depth != -1 && local->depth < current->scopeDepth) {
            break;
        }

        if (identifiersEqual(name, &local->name)) {
            error("Variable with this name already declared in this scope.");
        }
    }
    addLocal(*name);
}

static uint16_t parseVariable(const char* errorMessage) {
    consume(TOKEN_IDENTIFIER, errorMessage);

    declareVariable();
    if (current->scopeDepth > 0) return 0;

    return identifierConstant(&parser.previous);
}

static void markInitialized() {
    if (current->scopeDepth == 0) return;
    current->locals[current->localCount - 1].depth = current->scopeDepth;
}

static void defineVariable(uint16_t global) {
    if (current->scopeDepth > 0) {
        markInitialized();
        return;
    }
    if (global < 256) {
        emitBytes(OP_DEFINE_GLOBAL, global);
    } else {
            emitByte(OP_WIDE);
            emitByte(OP_DEFINE_GLOBAL);
            emitByte((uint8_t)(global >> 8));
            emitByte((uint8_t)global);
    }
}

static uint8_t argumentList() {
    uint8_t argCount = 0;
    if (!check(TOKEN_RIGHT_PAREN)) {
        do {
            expression();

            if (argCount == 255) {
                error("Cannot have more than 255 arguments.");
            }
            argCount++;
        } while (match(TOKEN_COMMA));
    }

    consume(TOKEN_RIGHT_PAREN, "Expect ')' after arguments.");
    return argCount;
}

static ParseRule* getRule(TokenType type) {
    return &rules[type];
}

ObjFunction* compile(const char* source) {
    initScanner(source);
    Compiler compiler;
    initCompiler(&compiler, TYPE_SCRIPT);

    parser.hadError = false;
    parser.panicMode = false;

    advance();

    while (!match(TOKEN_EOF)) {
        declaration();
    }

    ObjFunction* function = endCompiler();
    return parser.hadError ? NULL : function;
}

void markCompilerRoots() {
    Compiler* compiler = current;
    while (compiler != NULL) {
        markObject((Obj*)compiler->function);
        compiler = compiler->enclosing;
    }
}
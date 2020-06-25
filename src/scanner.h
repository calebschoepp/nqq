#ifndef nqq_scanner_h
#define nqq_scanner_h

typedef enum {
    // Single-character tokens.
    TOKEN_LEFT_PAREN, TOKEN_RIGHT_PAREN,
    TOKEN_LEFT_BRACE, TOKEN_RIGHT_BRACE,
    TOKEN_COMMA, TOKEN_DOT, TOKEN_MINUS,
    TOKEN_PLUS, TOKEN_SEMICOLON,
    TOKEN_SLASH, TOKEN_PERCENT,

    // One, two, or three character tokens.
    TOKEN_STAR, TOKEN_STAR_STAR, TOKEN_BANG,
    TOKEN_BANG_EQUAL, TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
    TOKEN_GREATER, TOKEN_GREATER_EQUAL,
    TOKEN_LESS, TOKEN_LESS_EQUAL, TOKEN_PLUS_EQUAL,
    TOKEN_MINUS_EQUAL, TOKEN_STAR_EQUAL, TOKEN_SLASH_EQUAL,
    TOKEN_PERCENT_EQUAL, TOKEN_STAR_STAR_EQUAL,

    // Literals.
    TOKEN_BASIC_STRING, TOKEN_IDENTIFIER, TOKEN_NUMBER,
    TOKEN_TEMPLATE_STRING, TOKEN_RAW_STRING,

    // Keywords.
    TOKEN_AND, TOKEN_BREAK, TOKEN_CONTINUE,
    TOKEN_ELSE, TOKEN_FALSE, TOKEN_FOR, TOKEN_FUN,
    TOKEN_IF, TOKEN_LET, TOKEN_NIL, TOKEN_OR,
    TOKEN_RETURN, TOKEN_TRUE, TOKEN_WHILE,

    TOKEN_ERROR,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
} Token;

void initScanner(const char* source);
Token scanToken();

#endif
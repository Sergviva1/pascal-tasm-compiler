#pragma once
#include <string>

enum class TokenType {
    PROGRAM, VAR, BEGIN_KW, END_KW,
    INTEGER_TYPE, REAL_TYPE, BOOLEAN_TYPE, CHAR_TYPE, STRING_TYPE,
    CONST, TYPE, PROCEDURE, FUNCTION, IF, THEN, ELSE, WHILE, DO,
    FOR, TO, DOWNTO, REPEAT, UNTIL, ARRAY, OF, RECORD, NOT, AND, OR,
    IDENTIFIER, DECIMAL_INTEGER,
    ASSIGN, COLON, SEMICOLON, COMMA, DOT, LEFT_PAREN, RIGHT_PAREN,
    PLUS, MINUS, STAR, SLASH, EQUAL, NOT_EQUAL, LESS, LESS_EQUAL,
    GREATER, GREATER_EQUAL,
    STRING_LITERAL, COMMENT,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
};

const char* tokenTypeName(TokenType type);

#include "Lexer.h"
#include <cctype>
#include <iostream>

// Таблица используется только для поиска ключевых слов.
// Это не хранилище токенов и не STL-контейнер для списка токенов.
// Если преподаватель запрещает любые STL-контейнеры вообще,
// замените её последовательностью сравнений строк.
static TokenType keywordType(const std::string& word, bool& found) {
    struct Keyword { const char* text; TokenType type; };
    static const Keyword keywords[] = {
        {"program", TokenType::PROGRAM}, {"var", TokenType::VAR},
        {"begin", TokenType::BEGIN_KW}, {"end", TokenType::END_KW},
        {"integer", TokenType::INTEGER_TYPE}, {"real", TokenType::REAL_TYPE},
        {"boolean", TokenType::BOOLEAN_TYPE}, {"char", TokenType::CHAR_TYPE},
        {"string", TokenType::STRING_TYPE}, {"const", TokenType::CONST},
        {"type", TokenType::TYPE}, {"procedure", TokenType::PROCEDURE},
        {"function", TokenType::FUNCTION}, {"if", TokenType::IF},
        {"then", TokenType::THEN}, {"else", TokenType::ELSE},
        {"while", TokenType::WHILE}, {"do", TokenType::DO},
        {"for", TokenType::FOR}, {"to", TokenType::TO},
        {"downto", TokenType::DOWNTO}, {"repeat", TokenType::REPEAT},
        {"until", TokenType::UNTIL}, {"array", TokenType::ARRAY},
        {"of", TokenType::OF}, {"record", TokenType::RECORD},
        {"not", TokenType::NOT}, {"and", TokenType::AND},
        {"or", TokenType::OR}
    };

    for (const Keyword& item : keywords) {
        std::string candidate = item.text;
        for (char& c : candidate)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (candidate == word) {
            found = true;
            return item.type;
        }
    }
    found = false;
    return TokenType::IDENTIFIER;
}

Lexer::Lexer(const std::string& text)
    : source(text), position(0), line(1), column(1), hasError(false) {}

char Lexer::current() const {
    return position < source.size() ? source[position] : '\0';
}

char Lexer::peek(std::size_t offset) const {
    return position + offset < source.size() ? source[position + offset] : '\0';
}

char Lexer::advance() {
    if (position >= source.size()) return '\0';
    char c = source[position++];
    if (c == '\n') {
        ++line;
        column = 1;
    } else {
        ++column;
    }
    return c;
}

void Lexer::skipWhitespace() {
    while (current() != '\0' &&
           std::isspace(static_cast<unsigned char>(current()))) {
        advance();
    }
}

void Lexer::addToken(TokenType type, const std::string& lexeme,
                     int tokenLine, int tokenColumn) {
    tokens.add(Token{type, lexeme, tokenLine, tokenColumn});
}

void Lexer::reportError(const std::string& message, int errorLine, int errorColumn) {
    std::cerr << "Лексическая ошибка: " << message
              << " (строка " << errorLine
              << ", столбец " << errorColumn << ")\n";
    hasError = true;
}

void Lexer::readIdentifierOrKeyword() {
    int startLine = line, startColumn = column;
    std::string lexeme;

    while (std::isalnum(static_cast<unsigned char>(current())) || current() == '_')
        lexeme += advance();

    std::string lowered = lexeme;
    for (char& c : lowered)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    bool found = false;
    TokenType type = keywordType(lowered, found);
    addToken(type, lexeme, startLine, startColumn);
}

void Lexer::readNumber() {
    int startLine = line, startColumn = column;
    std::string lexeme;

    while (std::isdigit(static_cast<unsigned char>(current())))
        lexeme += advance();

    addToken(TokenType::DECIMAL_INTEGER, lexeme, startLine, startColumn);
}

bool Lexer::analyze() {
    while (current() != '\0') {
        skipWhitespace();
        if (current() == '\0') break;

        int startLine = line, startColumn = column;
        char c = current();

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            readIdentifierOrKeyword();
            continue;
        }

        if (std::isdigit(static_cast<unsigned char>(c))) {
            readNumber();
            continue;
        }

        if (c == ':' && peek() == '=') {
            std::string op;
            op += advance(); op += advance();
            addToken(TokenType::ASSIGN, op, startLine, startColumn);
            continue;
        }

        TokenType type;
        bool recognized = true;
        switch (c) {
            case ':': type = TokenType::COLON; break;
            case ';': type = TokenType::SEMICOLON; break;
            case ',': type = TokenType::COMMA; break;
            case '.': type = TokenType::DOT; break;
            case '(': type = TokenType::LEFT_PAREN; break;
            case ')': type = TokenType::RIGHT_PAREN; break;
            case '+': type = TokenType::PLUS; break;
            case '-': type = TokenType::MINUS; break;
            case '*': type = TokenType::STAR; break;
            case '/': type = TokenType::SLASH; break;
            case '=': type = TokenType::EQUAL; break;
            case '<':
                advance();
                if (current() == '=') {
                    advance(); addToken(TokenType::LESS_EQUAL, "<=", startLine, startColumn);
                } else if (current() == '>') {
                    advance(); addToken(TokenType::NOT_EQUAL, "<>", startLine, startColumn);
                } else {
                    addToken(TokenType::LESS, "<", startLine, startColumn);
                }
                continue;
            case '>':
                advance();
                if (current() == '=') {
                    advance(); addToken(TokenType::GREATER_EQUAL, ">=", startLine, startColumn);
                } else {
                    addToken(TokenType::GREATER, ">", startLine, startColumn);
                }
                continue;
            default: recognized = false; type = TokenType::END_OF_FILE; break;
        }

        if (!recognized) {
            reportError(std::string("недопустимый символ '") + c + "'", startLine, startColumn);
            advance();
        } else {
            std::string lexeme(1, advance());
            addToken(type, lexeme, startLine, startColumn);
        }
    }

    addToken(TokenType::END_OF_FILE, "EOF", line, column);
    return !hasError;
}

const TokenStorage& Lexer::getTokens() const { return tokens; }
bool Lexer::failed() const { return hasError; }

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::PROGRAM: return "PROGRAM";
        case TokenType::VAR: return "VAR";
        case TokenType::BEGIN_KW: return "BEGIN";
        case TokenType::END_KW: return "END";
        case TokenType::INTEGER_TYPE: return "INTEGER_TYPE";
        case TokenType::REAL_TYPE: return "REAL_TYPE";
        case TokenType::BOOLEAN_TYPE: return "BOOLEAN_TYPE";
        case TokenType::CHAR_TYPE: return "CHAR_TYPE";
        case TokenType::STRING_TYPE: return "STRING_TYPE";
        case TokenType::CONST: return "CONST";
        case TokenType::TYPE: return "TYPE";
        case TokenType::PROCEDURE: return "PROCEDURE";
        case TokenType::FUNCTION: return "FUNCTION";
        case TokenType::IF: return "IF";
        case TokenType::THEN: return "THEN";
        case TokenType::ELSE: return "ELSE";
        case TokenType::WHILE: return "WHILE";
        case TokenType::DO: return "DO";
        case TokenType::FOR: return "FOR";
        case TokenType::TO: return "TO";
        case TokenType::DOWNTO: return "DOWNTO";
        case TokenType::REPEAT: return "REPEAT";
        case TokenType::UNTIL: return "UNTIL";
        case TokenType::ARRAY: return "ARRAY";
        case TokenType::OF: return "OF";
        case TokenType::RECORD: return "RECORD";
        case TokenType::NOT: return "NOT";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::DECIMAL_INTEGER: return "DECIMAL_INTEGER";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::COLON: return "COLON";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::DOT: return "DOT";
        case TokenType::LEFT_PAREN: return "LEFT_PAREN";
        case TokenType::RIGHT_PAREN: return "RIGHT_PAREN";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::EQUAL: return "EQUAL";
        case TokenType::NOT_EQUAL: return "NOT_EQUAL";
        case TokenType::LESS: return "LESS";
        case TokenType::LESS_EQUAL: return "LESS_EQUAL";
        case TokenType::GREATER: return "GREATER";
        case TokenType::GREATER_EQUAL: return "GREATER_EQUAL";
        case TokenType::STRING_LITERAL: return "STRING_LITERAL";
        case TokenType::COMMENT: return "COMMENT";
        case TokenType::END_OF_FILE: return "EOF";
    }
    return "UNKNOWN";
}

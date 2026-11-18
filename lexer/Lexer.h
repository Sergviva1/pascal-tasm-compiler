#pragma once
#include "TokenStorage.h"
#include <string>

class Lexer {
private:
    std::string source;
    std::size_t position;
    int line;
    int column;
    TokenStorage tokens;
    bool hasError;

    char current() const;
    char peek(std::size_t offset = 1) const;
    char advance();
    void skipWhitespace();
    void readIdentifierOrKeyword();
    void readNumber();
    void addToken(TokenType type, const std::string& lexeme, int tokenLine, int tokenColumn);
    void reportError(const std::string& message, int errorLine, int errorColumn);

public:
    explicit Lexer(const std::string& text);
    bool analyze();
    const TokenStorage& getTokens() const;
    bool failed() const;
};

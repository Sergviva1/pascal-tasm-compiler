#pragma once

#include "TokenStorage.h"
#include "ast_v1.hpp"
#include <iosfwd>
#include <string>

namespace pascal::parser {

struct Symbol {
    // В ЛР № 1 все символы — глобальные переменные типа integer.
    std::string name;
    int line = 0;
    int column = 0;
};

struct SymbolTable {
    Symbol entries[CAPACITY];
    int count = 0;
};

struct Result {
    Program program;
    SymbolTable symbols;
};

struct Diagnostic {
    int line = 0;
    int column = 0;
    std::string message;
};

// Вход заимствуется только на время вызова. При ошибке result пуст,
// при успехе diagnostic пуст. Выходы можно повторно использовать.
bool parse(const TokenStorage& tokens, Result& result, Diagnostic& diagnostic);
void printResult(std::ostream& out, const Result& result);
void printDiagnostic(std::ostream& out, const Diagnostic& diagnostic);

} // namespace pascal::parser

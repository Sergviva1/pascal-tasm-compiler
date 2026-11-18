#include "Lexer.h"
#include "parser.hpp"
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Использование: parser_cli <файл.pas>\n";
        return 1;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "Не удалось открыть файл: " << argv[1] << '\n';
        return 1;
    }
    std::string text;
    char buffer[4096];
    do {
        input.read(buffer, sizeof buffer);
        text.append(buffer, static_cast<std::size_t>(input.gcount()));
    } while (input);
    if (input.bad() || !input.eof()) {
        std::cerr << "Не удалось прочитать файл: " << argv[1] << '\n';
        return 1;
    }
    // Лексер использует NUL как признак конца строки.
    if (const auto nul = text.find('\0'); nul != std::string::npos) {
        std::size_t line = 1;
        std::size_t column = 1;
        for (std::size_t i = 0; i < nul; ++i) {
            if (text[i] == '\n') {
                ++line;
                column = 1;
            } else {
                ++column;
            }
        }
        std::cerr << line << ':' << column << ": недопустимый байт NUL\n";
        return 2;
    }
    Lexer lexer(text);
    if (!lexer.analyze()) return 2;

    pascal::parser::Result result;
    pascal::parser::Diagnostic diagnostic;
    if (!pascal::parser::parse(lexer.getTokens(), result, diagnostic)) {
        pascal::parser::printDiagnostic(std::cerr, diagnostic);
        return 3;
    }
    pascal::parser::printResult(std::cout, result);
    return 0;
}

#include "Lexer.h"
#include <fstream>
#include <iostream>
#include <sstream>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Использование: PascalLexer <имя_файла.pas>\n";
        return 1;
    }

    std::ifstream input(argv[1]);
    if (!input.is_open()) {
        std::cerr << "Ошибка: не удалось открыть файл " << argv[1] << "\n";
        return 1;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();

    Lexer lexer(buffer.str());
    bool success = lexer.analyze();

    const TokenStorage& tokens = lexer.getTokens();
    std::cout << "Категория\tЛексема\t\tСтрока\tСтолбец\n";
    std::cout << "------------------------------------------------------\n";

    for (std::size_t i = 0; i < tokens.size(); ++i) {
        const Token& token = tokens[i];
        std::cout << tokenTypeName(token.type) << "\t"
                  << token.lexeme << "\t\t"
                  << token.line << "\t"
                  << token.column << "\n";
    }

    return success ? 0 : 2;
}

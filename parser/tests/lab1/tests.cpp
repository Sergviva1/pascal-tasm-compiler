#include "parser.hpp"
#include <iostream>
#include <string>

using TT = TokenType;
using namespace pascal::parser;

namespace {
void header(TokenStorage& input) {
    input.add({TT::PROGRAM, "program", 1, 1});
    input.add({TT::IDENTIFIER, "Demo", 1, 9});
    input.add({TT::SEMICOLON, ";", 1, 13});
}

void assignment(TokenStorage& input, int line, const char* target, TT kind, const char* value,
                bool semicolon) {
    input.add({TT::IDENTIFIER, target, line, 3});
    input.add({TT::ASSIGN, ":=", line, 5});
    input.add({kind, value, line, 8});
    if (semicolon) input.add({TT::SEMICOLON, ";", line, 8 + static_cast<int>(std::string(value).size())});
}

void footer(TokenStorage& input, int line, bool eof = true) {
    input.add({TT::END_KW, "end", line, 1});
    input.add({TT::DOT, ".", line, 4});
    if (eof) input.add({TT::END_OF_FILE, "EOF", line + 1, 1});
}

void mandatoryCase(TokenStorage& input, int n) {
    header(input);
    input.add({TT::VAR, "var", 2, 1});
    input.add({TT::IDENTIFIER, n <= 2 ? "X" : "x", 2, 5});
    int colon = 6;
    if (n == 2) {
        input.add({TT::COMMA, ",", 2, 6});
        input.add({TT::IDENTIFIER, "Y", 2, 8});
        colon = 9;
    }
    if (n != 4) input.add({TT::COLON, ":", 2, colon});
    const int typeColumn = colon + (n == 4 ? 1 : 2);
    input.add({TT::INTEGER_TYPE, "integer", 2, typeColumn});
    input.add({TT::SEMICOLON, ";", 2, typeColumn + 7});
    if (n == 3) {
        input.add({TT::IDENTIFIER, "X", 3, 5});
        input.add({TT::COLON, ":", 3, 6});
        input.add({TT::INTEGER_TYPE, "integer", 3, 8});
        input.add({TT::SEMICOLON, ";", 3, 15});
    }
    const int blockLine = n == 3 ? 4 : 3;
    input.add({TT::BEGIN_KW, "begin", blockLine, 1});
    assignment(input, blockLine + 1, n <= 2 ? "X" : "x", TT::DECIMAL_INTEGER, "10", n <= 2);
    if (n == 2) assignment(input, blockLine + 2, "Y", TT::IDENTIFIER, "x", false);
    footer(input, blockLine + (n == 2 ? 3 : 2));
}

void many(TokenStorage& input, int variables, int statements, TT type = TT::INTEGER_TYPE) {
    header(input);
    if (variables > 0) input.add({TT::VAR, "var", 2, 1});
    for (int i = 0; i < variables; ++i) {
        const bool last = i + 1 == variables;
        const std::string name = "v" + std::to_string(i);
        const int after = 5 + static_cast<int>(name.size());
        input.add({TT::IDENTIFIER, name, i + 2, 5});
        if (!last) {
            input.add({TT::COMMA, ",", i + 2, after});
        } else {
            input.add({TT::COLON, ":", i + 2, after});
            input.add({type, type == TT::INTEGER_TYPE ? "integer" : "boolean", i + 2, after + 2});
            input.add({TT::SEMICOLON, ";", i + 2, after + 9});
        }
    }
    input.add({TT::BEGIN_KW, "begin", variables + 2, 1});
    for (int i = 0; i < statements; ++i) {
        assignment(input, variables + 3 + i, "v0", TT::DECIMAL_INTEGER, "1", i + 1 < statements);
    }
    footer(input, variables + statements + 3);
}

bool check(bool condition, const char* description) {
    if (!condition) std::cerr << "Не пройдена проверка: " << description << '\n';
    return condition;
}

bool empty(const Result& result) {
    return result.program.variableCount == 0 && result.symbols.count == 0 && result.program.statementCount == 0;
}

bool boundaries() {
    Result result;
    Diagnostic diagnostic;
    bool ok = true;
    {
        TokenStorage input;
        many(input, CAPACITY, CAPACITY);
        ok &= check(parse(input, result, diagnostic) && result.symbols.count == CAPACITY &&
                    result.program.variableCount == CAPACITY && result.program.statementCount == CAPACITY &&
                    result.symbols.entries[CAPACITY - 1].name == "v99" && diagnostic.message.empty(), "100 переменных и 100 присваиваний");
    }
    {
        TokenStorage input;
        many(input, CAPACITY + 1, 0);
        ok &= check(!parse(input, result, diagnostic) && empty(result) &&
                    diagnostic.line == 102 && diagnostic.column == 5 &&
                    diagnostic.message == "превышен лимит объявленных переменных: 100", "101-я переменная и сброс результата");
    }
    {
        TokenStorage input;
        many(input, 1, CAPACITY + 1);
        ok &= check(!parse(input, result, diagnostic) && empty(result) &&
                    diagnostic.line == 104 && diagnostic.column == 3 &&
                    diagnostic.message == "превышен лимит присваиваний: 100", "101-е присваивание");
    }
    {
        TokenStorage input;
        many(input, 0, 0);
        ok &= check(parse(input, result, diagnostic) && empty(result) && diagnostic.message.empty(), "пустой блок без var и сброс диагностики");
    }
    {
        TokenStorage input;
        many(input, 1, 0, TT::BOOLEAN_TYPE);
        ok &= check(!parse(input, result, diagnostic) && empty(result) &&
                    diagnostic.message == "ожидалось 'integer', получено 'boolean'", "тип за пределами ЛР № 1");
    }
    for (const char* value : {"00032767", "32768", "9999999999999999999999", "12x", ""}) {
        TokenStorage input;
        header(input);
        input.add({TT::BEGIN_KW, "begin", 2, 1});
        assignment(input, 3, "X", TT::DECIMAL_INTEGER, value, false);
        footer(input, 4);
        const bool parsed = parse(input, result, diagnostic);
        if (value[0] == '0') {
            ok &= check(parsed && result.program.statements[0].expression.number == 32767 &&
                        result.program.statements[0].target == "x", "граница литерала; имена не разрешаются в ЛР № 1");
        } else {
            ok &= check(!parsed && empty(result) && diagnostic.line == 3 && diagnostic.column == 8, "некорректный литерал");
        }
    }
    for (const bool eof : {false, true}) {
        TokenStorage input;
        header(input);
        input.add({TT::BEGIN_KW, "begin", 2, 1});
        footer(input, 3, eof);
        if (eof) input.add({TT::IDENTIFIER, "extra", 4, 2});
        ok &= check(!parse(input, result, diagnostic) && empty(result) &&
                    diagnostic.line == (eof ? 4 : 3) && diagnostic.column == (eof ? 2 : 4), "лишний токен или отсутствие EOF");
    }
    {
        TokenStorage input;
        ok &= check(!parse(input, result, diagnostic) && empty(result) &&
                    diagnostic.line == 1 && diagnostic.column == 1, "пустое хранилище");
    }
    {
        TokenStorage input;
        header(input);
        input.add({TT::BEGIN_KW, "begin", 2, 1});
        assignment(input, 3, "x", TT::DECIMAL_INTEGER, "1", false);
        input.add({TT::PLUS, "+", 3, 10});
        input.add({TT::DECIMAL_INTEGER, "2", 3, 12});
        footer(input, 4);
        ok &= check(!parse(input, result, diagnostic) && empty(result) &&
                    diagnostic.line == 3 && diagnostic.column == 10, "арифметика за пределами ЛР № 1");
    }
    // Пример передачи AST из PLAN: x := 7; y := x; x := 25.
    {
        TokenStorage input;
        header(input);
        input.add({TT::VAR, "var", 2, 1});
        input.add({TT::IDENTIFIER, "x", 2, 5});
        input.add({TT::COMMA, ",", 2, 6});
        input.add({TT::IDENTIFIER, "y", 2, 8});
        input.add({TT::COLON, ":", 2, 9});
        input.add({TT::INTEGER_TYPE, "integer", 2, 11});
        input.add({TT::SEMICOLON, ";", 2, 18});
        input.add({TT::BEGIN_KW, "begin", 3, 1});
        assignment(input, 4, "x", TT::DECIMAL_INTEGER, "7", true);
        assignment(input, 5, "y", TT::IDENTIFIER, "x", true);
        assignment(input, 6, "x", TT::DECIMAL_INTEGER, "25", false);
        footer(input, 7);
        const Program& ast = result.program;
        ok &= check(parse(input, result, diagnostic) && ast.variableCount == 2 && ast.variables[0] == "x" &&
                    ast.variables[1] == "y" && ast.statementCount == 3 && ast.statements[0].target == "x" &&
                    ast.statements[0].expression.kind == NUMBER && ast.statements[0].expression.number == 7 &&
                    ast.statements[1].target == "y" && ast.statements[1].expression.kind == VARIABLE &&
                    ast.statements[1].expression.name == "x" && ast.statements[2].target == "x" &&
                    ast.statements[2].expression.kind == NUMBER && ast.statements[2].expression.number == 25,
                    "порядок и значения узлов для генератора (без запуска ASM)");
    }
    return ok;
}
} // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) return 1;
    const std::string name = argv[1];
    if (name == "--boundaries") return boundaries() ? 0 : 1;
    if (name.size() != 2 || name[0] != 'T' || name[1] < '1' || name[1] > '4') return 1;

    TokenStorage input;
    mandatoryCase(input, name[1] - '0');
    Result result;
    Diagnostic diagnostic;
    if (parse(input, result, diagnostic)) {
        printResult(std::cout, result);
        return 0;
    }
    if (!empty(result)) return 1;
    printDiagnostic(std::cerr, diagnostic);
    return 3;
}

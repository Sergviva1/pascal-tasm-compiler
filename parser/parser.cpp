#include "parser.hpp"
#include <ostream>
#include <utility>

namespace pascal::parser {
namespace {
using TT = TokenType;

std::string normalize(std::string name) {
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return name;
}

class Parser {
public:
    Parser(const TokenStorage& tokens, Result& result, Diagnostic& diagnostic)
        : tokens_(tokens), result_(result), diagnostic_(diagnostic) {}

    bool program() {
        if (!expect(TT::PROGRAM, "'program'") || !expect(TT::IDENTIFIER, "имя программы") ||
            !expect(TT::SEMICOLON, "';'")) {
            return false;
        }
        if (accept(TT::VAR)) {
            do {
                if (!declaration()) return false;
            } while (at(TT::IDENTIFIER));
        }
        if (!block() || !expect(TT::DOT, "'.'") || !expect(TT::END_OF_FILE, "конец ввода")) {
            return false;
        }
        if (const Token* extra = current()) {
            return fail(*extra, "лишний токен после конца ввода: '" + extra->lexeme + "'");
        }
        return true;
    }

private:
    const TokenStorage& tokens_;
    Result& result_;
    Diagnostic& diagnostic_;
    std::size_t index_ = 0;

    const Token* current() const {
        return index_ < tokens_.size() ? &tokens_[index_] : nullptr;
    }

    bool at(TT type) const {
        const Token* token = current();
        return token && token->type == type;
    }

    bool accept(TT type) {
        if (!at(type)) return false;
        ++index_;
        return true;
    }

    bool fail(const Token& token, std::string message) {
        diagnostic_ = {token.line, token.column, std::move(message)};
        return false;
    }

    bool expected(const char* description) {
        const std::string prefix = std::string("ожидалось ") + description;
        if (const Token* token = current()) {
            if (token->type == TT::END_OF_FILE) {
                return fail(*token, prefix + ", получен конец ввода");
            }
            return fail(*token, prefix + ", получено '" + token->lexeme + "'");
        }
        const std::string message = prefix + ", поток токенов исчерпан без END_OF_FILE";
        if (index_ > 0) return fail(tokens_[index_ - 1], message);
        diagnostic_ = {1, 1, message};
        return false;
    }

    bool expect(TT type, const char* description) {
        return accept(type) || expected(description);
    }

    bool addSymbol(const Token& token) {
        std::string name = normalize(token.lexeme);
        SymbolTable& table = result_.symbols;
        for (int i = 0; i < table.count; ++i) {
            const Symbol& previous = table.entries[i];
            if (previous.name == name) {
                return fail(token, "повторное объявление '" + token.lexeme +
                                   "' (первое объявление: " + std::to_string(previous.line) + ":" +
                                   std::to_string(previous.column) + ")");
            }
        }
        Program& ast = result_.program;
        if (table.count >= CAPACITY || ast.variableCount >= CAPACITY) {
            return fail(token, "превышен лимит объявленных переменных: " + std::to_string(CAPACITY));
        }
        ast.variables[ast.variableCount++] = name;
        table.entries[table.count++] = {std::move(name), token.line, token.column};
        return true;
    }

    bool declaration() {
        do {
            const Token* name = current();
            if (!expect(TT::IDENTIFIER, "имя переменной") || !addSymbol(*name)) return false;
        } while (accept(TT::COMMA));
        return expect(TT::COLON, "':'") && expect(TT::INTEGER_TYPE, "'integer'") &&
               expect(TT::SEMICOLON, "';'");
    }

    bool block() {
        if (!expect(TT::BEGIN_KW, "'begin'")) return false;
        if (!at(TT::END_KW)) {
            do {
                if (!assignment()) return false;
            } while (accept(TT::SEMICOLON) && !at(TT::END_KW));
        }
        return expect(TT::END_KW, "'end'");
    }

    bool assignment() {
        const Token* target = current();
        if (!expect(TT::IDENTIFIER, "имя получателя присваивания") || !expect(TT::ASSIGN, "':='")) {
            return false;
        }
        Assignment node;
        node.target = normalize(target->lexeme);
        if (!value(node.expression)) return false;
        Program& ast = result_.program;
        if (ast.statementCount >= CAPACITY) {
            return fail(*target, "превышен лимит присваиваний: " + std::to_string(CAPACITY));
        }
        ast.statements[ast.statementCount++] = std::move(node);
        return true;
    }

    bool value(Expression& node) {
        const Token* token = current();
        if (accept(TT::IDENTIFIER)) {
            node.kind = VARIABLE;
            node.name = normalize(token->lexeme);
            return true;
        }
        if (!accept(TT::DECIMAL_INTEGER)) return expected("десятичное число или имя переменной");
        constexpr int maxInteger = 32767;
        int number = 0;
        if (token->lexeme.empty()) return fail(*token, "пустой десятичный литерал");
        for (char c : token->lexeme) {
            if (c < '0' || c > '9') {
                return fail(*token, "некорректный десятичный литерал '" + token->lexeme + "'");
            }
            const int digit = c - '0';
            if (number > (maxInteger - digit) / 10) {
                return fail(*token, "десятичный литерал вне диапазона 0…32767: '" + token->lexeme + "'");
            }
            number = number * 10 + digit;
        }
        node.kind = NUMBER;
        node.number = number;
        return true;
    }
};
} // namespace

bool parse(const TokenStorage& tokens, Result& result, Diagnostic& diagnostic) {
    result = Result{};
    diagnostic = Diagnostic{};
    Result candidate;
    if (!Parser(tokens, candidate, diagnostic).program()) return false;
    result = std::move(candidate);
    return true;
}

void printResult(std::ostream& out, const Result& result) {
    const Program& ast = result.program;
    out << "Program\n";
    for (int i = 0; i < ast.variableCount; ++i) {
        out << "  VarDeclaration: " << ast.variables[i] << " : integer\n";
    }
    out << "  Block\n";
    for (int i = 0; i < ast.statementCount; ++i) {
        const Assignment& node = ast.statements[i];
        out << "    Assignment\n      Target: " << node.target << '\n';
        if (node.expression.kind == NUMBER) {
            out << "      IntegerLiteral: " << node.expression.number << '\n';
        } else {
            out << "      Variable: " << node.expression.name << '\n';
        }
    }
    out << "Таблица символов\nИмя | Категория | Тип | Область | Объявление\n";
    for (int i = 0; i < result.symbols.count; ++i) {
        const Symbol& entry = result.symbols.entries[i];
        out << entry.name << " | переменная | integer | глобальная | " << entry.line << ':' << entry.column << '\n';
    }
}

void printDiagnostic(std::ostream& out, const Diagnostic& diagnostic) {
    out << diagnostic.line << ':' << diagnostic.column << ": " << diagnostic.message << '\n';
}

} // namespace pascal::parser

#include "json.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

using json = nlohmann::json;

void validateTokens(const json& tokens)
{
    if (!tokens.is_array())
    {
        throw std::runtime_error("Expected an array of tokens.");
    }
    if (tokens.empty())
    {
        throw std::runtime_error("The array of tokens is empty.");
    }

    int previousLine = 1;
    for (std::size_t i = 0; i < tokens.size(); ++i)
    {
        const auto& token = tokens.at(i);
        const std::string context = "Token " + std::to_string(i + 1) + ": ";
        if (!token.is_object())
        {
            throw std::runtime_error(context + "expected a JSON object.");
        }
        if (!token.contains("type") || !token.at("type").is_string() ||
            !token.contains("value") || !token.at("value").is_string())
        {
            throw std::runtime_error(context + "'type' and 'value' must be strings.");
        }
        if (!token.contains("line") || !token.at("line").is_number_integer())
        {
            throw std::runtime_error(context + "'line' must be an integer.");
        }
        // Check the range before converting to int, including unsigned JSON numbers.
        if (token.at("line") < 1 || token.at("line") > std::numeric_limits<int>::max())
        {
            throw std::runtime_error(context + "'line' must be a positive integer in int range.");
        }

        const int line = token.at("line").get<int>();
        const std::string location = "line " + std::to_string(line) + ": ";
        if (line < previousLine)
        {
            throw std::runtime_error(location + "token line numbers must not decrease.");
        }
        previousLine = line;

        const auto type = token.at("type").get<std::string>();
        const auto value = token.at("value").get<std::string>();
        if (type != "KEYWORD" && type != "IDENT" && type != "INT_LITERAL" &&
            type != "BOOL_LITERAL" && type != "OP" && type != "DELIM" && type != "EOF")
        {
            throw std::runtime_error(location + "unknown token type '" + type + "'.");
        }
        if (type == "EOF")
        {
            if (i + 1 != tokens.size())
            {
                throw std::runtime_error(location + "EOF must occur only at the end.");
            }
            if (!value.empty())
            {
                throw std::runtime_error(location + "EOF value must be empty.");
            }
        }
    }

    if (tokens.back().at("type").get<std::string>() != "EOF")
    {
        const int line = tokens.back().at("line").get<int>();
        throw std::runtime_error("line " + std::to_string(line) + ": expected a final EOF token.");
    }
}

// The input must pass validateTokens() before constructing a Parser.
class Parser
{
public:
    explicit Parser(const json& tokens) : tokens_(tokens) {}

    json parseProgram()
    {
        const auto name = parseProgramHeader();
        const auto block = parseBlock(true);
        expect("DELIM", ".");
        if (!isType("EOF"))
        {
            fail("unexpected token after program end");
        }
        return {{"node", "Program"}, {"name", name}, {"block", block}};
    }

    std::string parseProgramHeader()
    {
        expect("KEYWORD", "program");
        const std::string name = expectIdentifier();
        expect("DELIM", ";");
        return name;
    }

    json parseGlobalDeclarations()
    {
        json block = {{"node", "Block"}, {"consts", json::array()},
                      {"vars", json::array()}, {"procs", json::array()},
                      {"body", json::array()}};
        if (isToken("KEYWORD", "const"))
        {
            advance();
            if (!isType("IDENT"))
            {
                fail("expected a constant declaration");
            }
            do
            {
                block.at("consts").push_back(parseConstDeclaration());
            } while (isType("IDENT"));
        }
        if (isToken("KEYWORD", "var"))
        {
            advance();
            if (!isType("IDENT"))
            {
                fail("expected a variable declaration");
            }
            do
            {
                block.at("vars").push_back(parseVarDeclaration());
            } while (isType("IDENT"));
        }
        if (!isToken("KEYWORD", "procedure") && !isToken("KEYWORD", "begin"))
        {
            fail("expected 'procedure' or 'begin' after declarations");
        }
        return block;
    }

    const json& symbols() const
    {
        return symbols_;
    }

private:
    const json& tokens_;
    std::size_t pos_ = 0;
    json symbols_ = json::array();
    std::string scope_ = "global";
    // Separate scope identity from its serialized name (a procedure may be named global).
    std::size_t scopeId_ = 0;
    std::size_t nextScopeId_ = 1;
    std::vector<std::size_t> symbolScopes_;

    struct Expression
    {
        json node;
        std::string type;
        bool mutableVariable = false;
    };

    struct DeclaredName
    {
        std::string name;
        int line;
    };

    static std::string nameKey(std::string name)
    {
        // Pascal identifiers use ASCII; avoid locale-dependent case folding.
        for (char& c : name)
        {
            if (c >= 'A' && c <= 'Z')
            {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return name;
    }

    std::size_t declareSymbol(const DeclaredName& identifier, const std::string& kind,
                              const json& type, const json& value, bool isVar = false)
    {
        for (std::size_t i = 0; i < symbols_.size(); ++i)
        {
            if (symbolScopes_.at(i) == scopeId_ &&
                nameKey(symbols_.at(i).at("name").get<std::string>()) == nameKey(identifier.name))
            {
                failAt(identifier.line, "duplicate declaration of '" + identifier.name + "'");
            }
        }
        symbols_.push_back({{"name", identifier.name}, {"kind", kind}, {"type", type},
                            {"isVar", isVar}, {"scope", scope_}, {"value", value}});
        symbolScopes_.push_back(scopeId_);
        return symbols_.size() - 1;
    }

    json resolve(const DeclaredName& identifier) const
    {
        for (const auto wantedScope : {scopeId_, std::size_t{0}})
        {
            for (std::size_t i = 0; i < symbols_.size(); ++i)
            {
                if (symbolScopes_.at(i) == wantedScope &&
                    nameKey(symbols_.at(i).at("name").get<std::string>()) == nameKey(identifier.name))
                {
                    return symbols_.at(i);
                }
            }
        }
        failAt(identifier.line, "undeclared identifier '" + identifier.name + "'");
    }

    DeclaredName readDeclaredName()
    {
        const int line = peek().at("line").get<int>();
        return {expectIdentifier(), line};
    }

    std::string parseType()
    {
        if (!isToken("KEYWORD", "integer") && !isToken("KEYWORD", "boolean"))
        {
            fail("expected type 'integer' or 'boolean'");
        }
        return advance().at("value").get<std::string>();
    }

    json parseLiteral()
    {
        json value;
        if (isType("INT_LITERAL"))
        {
            const auto text = peek().at("value").get<std::string>();
            if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
            {
                fail("invalid integer literal '" + text + "'");
            }
            int number;
            try
            {
                number = std::stoi(text);
            }
            catch (const std::exception&)
            {
                fail("integer literal is outside int range");
            }
            value = number;
            advance();
        }
        else if (isType("BOOL_LITERAL"))
        {
            const auto text = peek().at("value").get<std::string>();
            if (text != "true" && text != "false")
            {
                fail("invalid boolean literal '" + text + "'");
            }
            value = (text == "true");
            advance();
        }
        else
        {
            fail("expected an integer or boolean constant literal");
        }
        return value;
    }

    json parseConstDeclaration()
    {
        const auto identifier = readDeclaredName();
        expect("OP", "=");
        const auto value = parseLiteral();
        const std::string type = value.is_boolean() ? "boolean" : "integer";
        expect("DELIM", ";");
        declareSymbol(identifier, "const", type, value);
        return {{"node", "ConstDecl"}, {"name", identifier.name},
                {"type", type}, {"value", value}};
    }

    json parseVarDeclaration()
    {
        std::vector<DeclaredName> identifiers;
        identifiers.push_back(readDeclaredName());
        while (isToken("DELIM", ","))
        {
            advance();
            identifiers.push_back(readDeclaredName());
        }
        expect("DELIM", ":");
        const auto type = parseType();
        expect("DELIM", ";");
        json names = json::array();
        for (const auto& identifier : identifiers)
        {
            declareSymbol(identifier, "var", type, nullptr);
            names.push_back(identifier.name);
        }
        return {{"node", "VarDecl"}, {"names", names}, {"type", type}};
    }

    json parseBlock(bool allowProcedures)
    {
        json block = parseGlobalDeclarations();
        while (isToken("KEYWORD", "procedure"))
        {
            if (!allowProcedures)
            {
                fail("nested procedures are not supported");
            }
            block.at("procs").push_back(parseProcedure());
        }
        expect("KEYWORD", "begin");
        block.at("body") = parseStatementList();
        expect("KEYWORD", "end");
        return block;
    }

    json parseProcedure()
    {
        expect("KEYWORD", "procedure");
        const auto identifier = readDeclaredName();
        const auto procIndex = declareSymbol(identifier, "proc", nullptr, nullptr);
        scope_ = identifier.name;
        scopeId_ = nextScopeId_++;
        expect("DELIM", "(");
        json params = json::array();
        json signature = json::array();
        if (!isToken("DELIM", ")"))
        {
            do
            {
                bool isVar = false;
                if (isToken("KEYWORD", "var"))
                {
                    advance();
                    isVar = true;
                }
                std::vector<DeclaredName> identifiers{readDeclaredName()};
                while (isToken("DELIM", ","))
                {
                    advance();
                    identifiers.push_back(readDeclaredName());
                }
                expect("DELIM", ":");
                const auto type = parseType();
                json names = json::array();
                for (const auto& parameter : identifiers)
                {
                    declareSymbol(parameter, "param", type, nullptr, isVar);
                    names.push_back(parameter.name);
                    signature.push_back({{"type", type}, {"isVar", isVar}});
                }
                params.push_back({{"node", "Param"}, {"names", names},
                                  {"type", type}, {"isVar", isVar}});
                if (!isToken("DELIM", ";"))
                {
                    break;
                }
                advance();
            } while (true);
        }
        expect("DELIM", ")");
        expect("DELIM", ";");
        // Complete the signature before the body to allow recursion.
        symbols_.at(procIndex)["paramsSignature"] = signature;
        const auto block = parseBlock(false);
        expect("DELIM", ";");
        scope_ = "global";
        scopeId_ = 0;
        return {{"node", "ProcDecl"}, {"name", identifier.name},
                {"params", params}, {"block", block}};
    }

    json parseStatementList()
    {
        json statements = json::array();
        // Empty blocks and a trailing semicolon are accepted as in Pascal.
        if (isToken("KEYWORD", "end"))
        {
            return statements;
        }
        statements.push_back(parseStatement());
        while (isToken("DELIM", ";"))
        {
            advance();
            if (isToken("KEYWORD", "end"))
            {
                break;
            }
            statements.push_back(parseStatement());
        }
        return statements;
    }

    json parseStatement()
    {
        if (isToken("KEYWORD", "begin"))
        {
            advance();
            const auto statements = parseStatementList();
            expect("KEYWORD", "end");
            return {{"node", "Compound"}, {"statements", statements}};
        }
        if (isToken("KEYWORD", "if"))
        {
            advance();
            const auto left = parseExpression();
            const int line = peek().at("line").get<int>();
            const auto op = peek().at("value").get<std::string>();
            if (!isType("OP") || (op != "=" && op != "<>" && op != "<" &&
                op != ">" && op != "<=" && op != ">="))
            {
                fail("expected a comparison operator");
            }
            advance();
            const auto right = parseExpression();
            if (left.type != right.type)
            {
                failAt(line, "comparison operands have incompatible types");
            }
            expect("KEYWORD", "then");
            const auto thenBranch = parseStatement();
            json elseBranch = nullptr;
            if (isToken("KEYWORD", "else"))
            {
                advance();
                elseBranch = parseStatement();
            }
            return {{"node", "If"},
                    {"condition", {{"node", "Compare"}, {"op", op},
                                   {"left", left.node}, {"right", right.node}}},
                    {"thenBranch", thenBranch}, {"elseBranch", elseBranch}};
        }
        if (!isType("IDENT"))
        {
            fail("expected a statement");
        }
        const auto identifier = readDeclaredName();
        const auto symbol = resolve(identifier);
        const auto kind = symbol.at("kind").get<std::string>();
        const auto name = symbol.at("name").get<std::string>();
        if (isToken("OP", ":="))
        {
            if (kind != "var" && kind != "param")
            {
                failAt(identifier.line, "cannot assign to '" + identifier.name + "'");
            }
            advance();
            const auto expr = parseExpression();
            if (expr.type != symbol.at("type").get<std::string>())
            {
                failAt(identifier.line, "assignment has incompatible types");
            }
            return {{"node", "Assign"}, {"target", name}, {"expr", expr.node}};
        }
        if (kind != "proc")
        {
            failAt(identifier.line, "'" + identifier.name + "' is not a procedure");
        }
        std::vector<Expression> arguments;
        if (isToken("DELIM", "("))
        {
            advance();
            if (!isToken("DELIM", ")"))
            {
                arguments.push_back(parseExpression());
                while (isToken("DELIM", ","))
                {
                    advance();
                    arguments.push_back(parseExpression());
                }
            }
            expect("DELIM", ")");
        }
        const auto& signature = symbol.at("paramsSignature");
        if (arguments.size() != signature.size())
        {
            failAt(identifier.line, "wrong number of arguments to '" + name + "'");
        }
        json args = json::array();
        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            if (arguments.at(i).type != signature.at(i).at("type").get<std::string>())
            {
                failAt(identifier.line, "argument " + std::to_string(i + 1) + " has incompatible type");
            }
            if (signature.at(i).at("isVar").get<bool>() && !arguments.at(i).mutableVariable)
            {
                failAt(identifier.line, "argument " + std::to_string(i + 1) +
                       " for var-parameter must be a mutable variable");
            }
            args.push_back(arguments.at(i).node);
        }
        return {{"node", "ProcCall"}, {"name", name}, {"args", args}};
    }

    Expression parseExpression()
    {
        auto left = parseTerm();
        while (isToken("OP", "+") || isToken("OP", "-"))
        {
            const int line = peek().at("line").get<int>();
            const auto op = advance().at("value").get<std::string>();
            left = binary(left, op, parseTerm(), line);
        }
        return left;
    }

    Expression parseTerm()
    {
        auto left = parseFactor();
        while (isToken("OP", "*") || isToken("KEYWORD", "div") || isToken("KEYWORD", "mod"))
        {
            const int line = peek().at("line").get<int>();
            const auto op = advance().at("value").get<std::string>();
            left = binary(left, op, parseFactor(), line);
        }
        return left;
    }

    Expression binary(const Expression& left, const std::string& op,
                      const Expression& right, int line) const
    {
        if (left.type != "integer" || right.type != "integer")
        {
            failAt(line, "arithmetic operands must be integer");
        }
        return {{{"node", "BinOp"}, {"op", op}, {"left", left.node}, {"right", right.node}},
                "integer", false};
    }

    Expression parseFactor()
    {
        if (isToken("OP", "-"))
        {
            const int line = advance().at("line").get<int>();
            const auto operand = parseFactor();
            if (operand.type != "integer")
            {
                failAt(line, "unary minus requires an integer");
            }
            return {{{"node", "UnaryMinus"}, {"operand", operand.node}}, "integer", false};
        }
        if (isToken("DELIM", "("))
        {
            advance();
            const auto expr = parseExpression();
            expect("DELIM", ")");
            return expr;
        }
        if (isType("IDENT"))
        {
            const auto identifier = readDeclaredName();
            const auto symbol = resolve(identifier);
            const auto kind = symbol.at("kind").get<std::string>();
            if (kind == "proc")
            {
                failAt(identifier.line, "a procedure cannot be used as a value");
            }
            return {{{"node", kind == "const" ? "ConstRef" : "VarRef"},
                     {"name", symbol.at("name").get<std::string>()}},
                    symbol.at("type").get<std::string>(), kind != "const"};
        }
        if (isType("INT_LITERAL") || isType("BOOL_LITERAL"))
        {
            const auto literal = parseLiteral();
            const auto type = literal.is_boolean() ? "boolean" : "integer";
            return {{{"node", literal.is_boolean() ? "BoolLiteral" : "IntLiteral"},
                     {"value", literal}}, type, false};
        }
        fail("expected an expression");
    }

    [[noreturn]] static void failAt(int line, const std::string& message)
    {
        throw std::runtime_error("line " + std::to_string(line) + ": " + message);
    }

    const json& peek() const
    {
        return tokens_.at(pos_);
    }

    bool isType(const std::string& type) const
    {
        return peek().at("type").get<std::string>() == type;
    }

    bool isToken(const std::string& type, const std::string& value) const
    {
        return isType(type) && peek().at("value").get<std::string>() == value;
    }

    [[noreturn]] void fail(const std::string& message) const
    {
        throw std::runtime_error("line " + std::to_string(peek().at("line").get<int>()) +
                                 ": " + message);
    }

    const json& advance()
    {
        const auto& token = peek();
        // Never move past EOF: truncated input must produce a syntax error,
        // not an out-of-range access on the next peek().
        if (!isType("EOF"))
        {
            ++pos_;
        }
        return token;
    }

    void expect(const std::string& type, const std::string& value)
    {
        if (!isToken(type, value))
        {
            fail("expected '" + value + "'");
        }
        advance();
    }

    std::string expectIdentifier()
    {
        if (!isType("IDENT"))
        {
            fail("expected an identifier");
        }
        return advance().at("value").get<std::string>();
    }
};

#ifndef PARSER_NO_MAIN
int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <tokens.lst>\n";
        return 1;
    }

    try
    {
        std::ifstream jsonFile(argv[1]);
        if (!jsonFile.is_open())
        {
            throw std::runtime_error(std::string("Could not open the file: ") + argv[1]);
        }

        json jsonData;
        try
        {
            jsonFile >> jsonData;
            jsonFile >> std::ws;
            if (jsonFile.peek() != std::char_traits<char>::eof())
            {
                throw std::runtime_error("Unexpected data after the JSON token array");
            }
        }
        catch (const json::parse_error& error)
        {
            throw std::runtime_error(std::string("JSON parse error: ") + error.what());
        }

        validateTokens(jsonData);

        Parser parser(jsonData);
        const json tree = parser.parseProgram();

        // All syntax and semantic checks finish before any output is opened.
        std::ofstream treeFile("tree.json");
        if (!treeFile)
        {
            throw std::runtime_error("Could not open tree.json for writing");
        }
        std::ofstream symbolsFile("symbols.tab");
        if (!symbolsFile)
        {
            throw std::runtime_error("Could not open symbols.tab for writing");
        }
        treeFile << tree.dump(2);
        symbolsFile << parser.symbols().dump(2);
        treeFile.close();
        symbolsFile.close();
        if (!treeFile || !symbolsFile)
        {
            throw std::runtime_error("Could not write parser output files");
        }
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "parser: " << error.what() << '\n';
        return 1;
    }
}
#endif

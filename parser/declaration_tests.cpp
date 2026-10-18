// Build from the project root:
// g++ -std=c++17 -Wall -Wextra -Wpedantic -I. parser/declaration_tests.cpp -o build/parser_declaration_tests.exe
#define PARSER_NO_MAIN
#include "main.cpp"

void require(bool condition, const std::string& message)
{
    if (!condition)
    {
        throw std::runtime_error(message);
    }
}

json token(const std::string& type, const std::string& value, int line = 1)
{
    return {{"type", type}, {"value", value}, {"line", line}};
}

json stream(const json& declarations)
{
    json result = json::array({token("KEYWORD", "program"), token("IDENT", "Test"),
                               token("DELIM", ";")});
    for (const auto& item : declarations)
    {
        result.push_back(item);
    }
    result.push_back(token("KEYWORD", "begin", 10));
    result.push_back(token("EOF", "", 10));
    return result;
}

void reject(const json& declarations, const std::string& expected)
{
    const auto tokens = stream(declarations);
    validateTokens(tokens);
    Parser parser(tokens);
    parser.parseProgramHeader();
    try
    {
        parser.parseGlobalDeclarations();
    }
    catch (const std::runtime_error& error)
    {
        require(std::string(error.what()).find(expected) != std::string::npos,
                "unexpected diagnostic: " + std::string(error.what()));
        return;
    }
    throw std::runtime_error("expected rejection: " + expected);
}

int main()
{
    try
    {
        std::ifstream input("tests/tokens.lst");
        require(input.is_open(), "run tests from the project root");
        json tokens;
        input >> tokens;
        validateTokens(tokens);
        Parser parser(tokens);
        require(parser.parseProgramHeader() == "Example", "program name");
        const auto block = parser.parseGlobalDeclarations();
        std::ifstream treeInput("tests/tree.json");
        json tree;
        treeInput >> tree;
        require(block.at("consts") == tree.at("block").at("consts"), "fixture constants");
        require(block.at("vars") == tree.at("block").at("vars"), "fixture variables");
        std::ifstream symbolsInput("tests/symbols.tab");
        json symbols;
        symbolsInput >> symbols;
        require(parser.symbols().size() == 4, "four global data symbols");
        for (const auto& symbol : parser.symbols())
        {
            bool found = false;
            for (const auto& expected : symbols)
            {
                found = found || symbol == expected;
            }
            require(found, "unexpected symbol: " + symbol.dump());
        }

        const auto empty = stream(json::array());
        Parser emptyParser(empty);
        emptyParser.parseProgramHeader();
        require(emptyParser.parseGlobalDeclarations().at("consts").empty(), "empty declarations");
        require(emptyParser.symbols().empty(), "empty symbols");

        const auto booleans = stream({token("KEYWORD", "const"), token("IDENT", "Enabled"),
                                     token("OP", "="), token("BOOL_LITERAL", "false"), token("DELIM", ";")});
        Parser boolParser(booleans);
        boolParser.parseProgramHeader();
        const auto boolBlock = boolParser.parseGlobalDeclarations();
        require(boolBlock.at("consts").at(0).at("value").is_boolean(), "typed boolean constant");
        require(boolParser.symbols().at(0).at("value") == false, "false constant");

        reject(json::array({token("KEYWORD", "const")}), "expected a constant declaration");
        reject(json::array({token("KEYWORD", "var")}), "expected a variable declaration");
        reject({token("KEYWORD", "var"), token("IDENT", "A"), token("DELIM", ","),
                token("IDENT", "a", 3), token("DELIM", ":", 3),
                token("KEYWORD", "integer", 3), token("DELIM", ";", 3)},
               "line 3: duplicate declaration");
        reject({token("KEYWORD", "const"), token("IDENT", "X"), token("OP", "="),
                token("INT_LITERAL", "10"), token("DELIM", ";"), token("KEYWORD", "var"),
                token("IDENT", "x"), token("DELIM", ":"), token("KEYWORD", "boolean"),
                token("DELIM", ";")}, "duplicate declaration");
        reject({token("KEYWORD", "var"), token("IDENT", "x"), token("DELIM", ":"),
                token("KEYWORD", "real"), token("DELIM", ";")}, "expected type");
        reject({token("KEYWORD", "var"), token("IDENT", "x"), token("DELIM", ":"),
                token("KEYWORD", "integer")}, "expected ';'");
        for (const auto& text : {"999999999999999999999999", "12x", "-1", ""})
        {
            reject({token("KEYWORD", "const"), token("IDENT", "X"), token("OP", "="),
                    token("INT_LITERAL", text), token("DELIM", ";")}, "integer literal");
        }
        reject({token("KEYWORD", "const"), token("IDENT", "X"), token("OP", "="),
                token("BOOL_LITERAL", "TRUE"), token("DELIM", ";")}, "invalid boolean literal");
        reject({token("KEYWORD", "const"), token("IDENT", "X"), token("OP", "="),
                token("IDENT", "Other"), token("DELIM", ";")}, "expected an integer or boolean");
        reject(json::array({token("KEYWORD", "end")}), "expected 'procedure' or 'begin'");
        std::cout << "Parser declaration checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "declaration_tests: " << error.what() << '\n';
        return 1;
    }
}

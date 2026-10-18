// Test-only token builder: words/punctuation in snippets MUST be space separated.
// This is not a lexer and is never used by the compiler.
#define PARSER_NO_MAIN
#include "main.cpp"

#include <algorithm>
#include <sstream>

void require(bool condition, const std::string& message)
{
    if (!condition) throw std::runtime_error(message);
}

json load(const std::string& path)
{
    std::ifstream input(path);
    require(input.is_open(), "cannot open " + path);
    json data;
    input >> data;
    return data;
}

json testTokens(const std::string& text)
{
    const std::vector<std::string> keywords = {
        "program", "const", "var", "integer", "boolean", "procedure", "begin", "end",
        "if", "then", "else", "div", "mod", "real", "while"};
    const std::vector<std::string> ops = {":=", "+", "-", "*", "=", "<>", "<", ">", "<=", ">="};
    const std::vector<std::string> delims = {";", ",", ":", "(", ")", "."};
    std::istringstream lines(text);
    json tokens = json::array();
    int lineNumber = 0;
    for (std::string line; std::getline(lines, line);)
    {
        ++lineNumber;
        std::istringstream words(line);
        for (std::string word; words >> word;)
        {
            std::string type = "IDENT";
            if (std::find(keywords.begin(), keywords.end(), word) != keywords.end()) type = "KEYWORD";
            else if (std::find(ops.begin(), ops.end(), word) != ops.end()) type = "OP";
            else if (std::find(delims.begin(), delims.end(), word) != delims.end()) type = "DELIM";
            else if (word == "true" || word == "false") type = "BOOL_LITERAL";
            else if (word.find_first_not_of("0123456789") == std::string::npos) type = "INT_LITERAL";
            tokens.push_back({{"type", type}, {"value", word}, {"line", lineNumber}});
        }
    }
    tokens.push_back({{"type", "EOF"}, {"value", ""}, {"line", std::max(1, lineNumber)}});
    return tokens;
}

json parse(const std::string& text)
{
    const auto tokens = testTokens(text);
    validateTokens(tokens);
    Parser parser(tokens);
    return parser.parseProgram();
}

int rejected = 0;
void reject(const std::string& text, const std::string& diagnostic)
{
    try { parse(text); }
    catch (const std::runtime_error& error)
    {
        const std::string message = error.what();
        require(message.find("line ") == 0 && message.find(diagnostic) != std::string::npos,
                "unexpected diagnostic: " + message + "; expected " + diagnostic);
        ++rejected;
        return;
    }
    throw std::runtime_error("unexpected acceptance: " + text);
}

void compareSymbols(const json& actual, const json& expected)
{
    require(actual.size() == expected.size(), "symbol count differs");
    for (const auto& symbol : actual)
    {
        require(std::find(expected.begin(), expected.end(), symbol) != expected.end(),
                "unexpected symbol: " + symbol.dump());
    }
}

int main(int argc, char* argv[])
{
    try
    {
        const auto tokens = load("tests/tokens.lst");
        validateTokens(tokens);
        Parser parser(tokens);
        require(parser.parseProgram() == load("tests/tree.json"), "fixture AST mismatch");
        compareSymbols(parser.symbols(), load("tests/symbols.tab"));
        if (argc == 2)
        {
            require(load(std::string(argv[1]) + "/tree.json") == load("tests/tree.json"), "CLI AST mismatch");
            compareSymbols(load(std::string(argv[1]) + "/symbols.tab"), load("tests/symbols.tab"));
        }

        const auto empty = parse("program Empty ; begin end .");
        require(empty.at("block").at("body") == json::array(), "empty body must be array");
        const auto arithmetic = parse("program P ; var x : integer ; begin x := - 1 + 2 * 3 div 4 mod 5 - ( 6 + 7 ) ; end .");
        const auto& expr = arithmetic.at("block").at("body").at(0).at("expr");
        require(expr.at("op") == "-" && expr.at("left").at("op") == "+", "additive associativity");
        require(expr.at("left").at("left").at("node") == "UnaryMinus", "unary minus");
        const auto& term = expr.at("left").at("right");
        require(term.at("op") == "mod" && term.at("left").at("op") == "div" &&
                term.at("left").at("left").at("op") == "*", "precedence and term associativity");

        const auto mixedCase = parse("program P ; const Limit = 10 ; var Flag : boolean ; begin fLAG := false ; if limit = LIMIT then FLAG := true end .");
        const auto& body = mixedCase.at("block").at("body");
        require(body.at(0).at("target") == "Flag", "canonical assignment name");
        require(body.at(1).at("condition").at("left").at("name") == "Limit", "canonical reference name");
        require(body.at(1).at("elseBranch").is_null(), "missing else must be null");
        const auto dangling = parse("program P ; var x : integer ; begin if x = 1 then if x = 2 then x := 3 else x := 4 end .");
        const auto& branch = dangling.at("block").at("body").at(0);
        require(branch.at("elseBranch").is_null() && !branch.at("thenBranch").at("elseBranch").is_null(), "dangling else");
        for (const auto& op : {"=", "<>", "<", ">", "<=", ">="})
        {
            parse(std::string("program P ; var x : integer ; begin if x ") + op + " 2 then begin x := 1 ; end else begin end end .");
        }
        parse("program P ; procedure Q ( ) ; begin end ; begin q ; Q ( ) end .");
        parse("program P ; var x : integer ; procedure Q ( a , b : integer ; var c , d : integer ) ; begin c := a ; d := b end ; begin q ( 1 , 2 , x , x ) end .");
        parse("program P ; var x : integer ; procedure Q ( var a : integer ) ; begin Q ( a ) end ; begin Q ( x ) end .");
        parse("program P ; var x : integer ; procedure Q ( var a : integer ) ; begin end ; procedure R ( var b : integer ) ; begin Q ( b ) end ; begin R ( x ) end .");
        parse("program P ; var x : integer ; procedure Q ( x : boolean ) ; const C = true ; var y : boolean ; begin y := C ; x := y end ; begin x := 1 ; Q ( false ) end .");
        parse("program P ; procedure global ( a : integer ) ; begin end ; begin global ( 1 ) end .");

        reject("program P ; begin x := 1 end .", "undeclared identifier");
        reject("program P ; begin Q end .", "undeclared identifier");
        reject("program P ; const X = 1 ; var x : integer ; begin end .", "duplicate declaration");
        reject("program P ; var x , X : integer ; begin end .", "duplicate declaration");
        reject("program P ; var x : integer ; begin x := true end .", "assignment has incompatible types");
        reject("program P ; var x : boolean ; begin x := 1 end .", "assignment has incompatible types");
        reject("program P ; const C = 1 ; begin C := 2 end .", "cannot assign");
        reject("program P ; begin if 1 = true then begin end end .", "comparison operands");
        reject("program P ; var b : boolean ; begin b := true + false end .", "arithmetic operands");
        reject("program P ; var b : boolean ; begin b := - true end .", "unary minus");
        reject("program P ; var x : integer ; begin x := 1 * false end .", "arithmetic operands");
        reject("program P ; procedure Q ( a : integer ) ; begin end ; begin Q end .", "wrong number");
        reject("program P ; procedure Q ( ) ; begin end ; begin Q ( 1 ) end .", "wrong number");
        reject("program P ; procedure Q ( a : integer ) ; begin end ; begin Q ( true ) end .", "incompatible type");
        const std::string varPrefix = "program P ; const C = 1 ; var x : integer ; procedure Q ( var a : integer ) ; begin end ; begin Q ( ";
        for (const auto& arg : {"1", "C", "x + 1", "- x"})
        {
            reject(varPrefix + arg + " ) end .", "mutable variable");
        }
        reject("program P ; procedure Q ( ) ; begin end ; var x : integer ; begin end .", "expected 'begin'");
        reject("program P ; procedure Q ( ) ; begin end ; begin Q := 1 end .", "cannot assign");
        reject("program P ; var x : integer ; procedure Q ( ) ; begin end ; begin x := Q end .", "cannot be used as a value");
        reject("program P ; var x : integer ; begin x ( ) end .", "not a procedure");
        reject("program P ; procedure Q ( x , X : integer ) ; begin end ; begin end .", "duplicate declaration");
        reject("program P ; procedure Q ( x : integer ) ; var X : integer ; begin end ; begin end .", "duplicate declaration");
        reject("program P ; procedure Q ( ) ; begin end ; procedure q ( ) ; begin end ; begin end .", "duplicate declaration");
        reject("program P ; procedure Q ( ) ; procedure R ( ) ; begin end ; begin end ; begin end .", "nested procedures");
        reject("program P ; procedure Q ( ) ; var local : integer ; begin end ; begin local := 1 end .", "undeclared identifier");
        reject("program P ; procedure Q ( ) ; var local : integer ; begin end ; procedure R ( ) ; begin local := 1 end ; begin end .", "undeclared identifier");
        reject("program P ; procedure Q ( ; ) ; begin end ; begin end .", "expected an identifier");
        reject("program P ; begin end . extra", "unexpected token");
        reject("program P ; var x : integer ; begin x := ( 1 + 2 end .", "expected ')'");
        reject("program P ; begin if true then begin end end .", "comparison operator");
        reject("program P ; var x : integer ; begin if x = 1 then x := 2 ; else x := 3 end .", "expected a statement");
        reject("program P ; var x : integer ; begin x := 1 x := 2 end .", "expected 'end'");
        reject("program P ; var x : integer ; begin x := end .", "expected an expression");
        reject("program P ; begin while end .", "expected a statement");
        reject("program P ; begin end", "expected '.'");
        reject("program P ;\nbegin\nx := 1\nend .", "line 3: undeclared");
        reject("program P ; var x : integer ; begin x := 999999999999999999999999 end .", "outside int range");

        // Every proper token prefix of the fixture must fail safely at EOF.
        for (std::size_t length = 0; length + 1 < tokens.size(); ++length)
        {
            json prefix = json::array();
            for (std::size_t i = 0; i < length; ++i) prefix.push_back(tokens.at(i));
            prefix.push_back({{"type", "EOF"}, {"value", ""}, {"line", tokens.at(length).at("line")}});
            validateTokens(prefix);
            Parser partial(prefix);
            bool failed = false;
            try { partial.parseProgram(); }
            catch (const std::runtime_error&) { failed = true; }
            require(failed, "truncated input was accepted");
        }
        std::cout << "Parser checks passed: fixture AST/symbols, positive cases, " << rejected
                  << " negative cases, all truncated fixture prefixes\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "parser_tests: " << error.what() << '\n';
        return 1;
    }
}

#include "json.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

using json = nlohmann::json;
namespace fs = std::filesystem;

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

json load(const fs::path& path) {
    std::ifstream input(path);
    require(input.is_open(), "cannot open " + path.string());
    return json::parse(input);
}

// This checks the reference example, not arbitrary Pascal programs.
void check(const fs::path& directory) {
    std::ifstream source(directory / "example.pas");
    require(source.is_open(), "cannot open example.pas");
    std::size_t lines = 0;
    for (std::string line; std::getline(source, line);) {
        ++lines;
    }

    const auto tokens = load(directory / "tokens.lst");
    require(tokens.is_array() && !tokens.empty(), "tokens must be a nonempty array");
    int previousLine = 1;
    std::size_t eofCount = 0;
    const json types = {"KEYWORD", "IDENT", "INT_LITERAL", "BOOL_LITERAL", "OP", "DELIM", "EOF"};
    for (const auto& token : tokens) {
        require(token.at("type").is_string(), "token type must be a string");
        require(token.at("value").is_string(), "token value must be a string");
        require(token.at("line").is_number_integer(), "token line must be an integer");
        require(std::find(types.begin(), types.end(), token.at("type")) != types.end(),
                "unknown token type");
        const int line = token.at("line").get<int>();
        require(line >= previousLine && static_cast<std::size_t>(line) <= lines + 1,
                "invalid token line");
        previousLine = line;
        if (token.at("type") == "EOF") {
            ++eofCount;
            require(token.at("value") == "", "EOF value must be empty");
        }
    }
    require(eofCount == 1 && tokens.back().at("type") == "EOF", "expected one final EOF");
    require(tokens.at(1).at("value") == "Example", "unexpected program name in tokens");

    const auto tree = load(directory / "tree.json");
    require(tree.at("node") == "Program" && tree.at("name") == "Example", "invalid Program");
    const auto& block = tree.at("block");
    require(block.at("node") == "Block", "invalid Block");
    require(block.at("consts").at(0).at("value").is_number_integer(), "integer const must be a number");
    require(block.at("consts").at(0).at("value") == 10, "unexpected Limit");
    const auto& proc = block.at("procs").at(0);
    require(proc.at("name") == "Compare", "unexpected procedure");
    require(proc.at("params").at(0).at("isVar") == false &&
            proc.at("params").at(1).at("isVar") == true, "invalid parameter modes");
    const auto& branch = proc.at("block").at("body").at(0);
    require(branch.at("node") == "If" && branch.at("condition").at("op") == "<", "invalid condition");
    require(branch.at("thenBranch").at("expr").at("value").is_boolean(), "boolean literal must be bool");
    require(branch.at("thenBranch").at("expr").at("value") == true &&
            branch.at("elseBranch").at("expr").at("value") == false, "invalid branches");
    require(block.at("body").at(2).at("name") == "Compare", "missing procedure call");

    const auto symbols = load(directory / "symbols.tab");
    require(symbols.is_array() && symbols.size() == 7, "expected seven symbols");
    json index = json::object();
    for (const auto& symbol : symbols) {
        const auto key = symbol.at("scope").get<std::string>() + "/" + symbol.at("name").get<std::string>();
        require(!index.contains(key), "duplicate symbol: " + key);
        require(symbol.at("isVar").is_boolean(), "isVar must be boolean");
        require(symbol.contains("type") && symbol.contains("value") && symbol.contains("kind"),
                "missing symbol fields");
        index[key] = symbol;
    }
    require(index.at("global/Limit").at("value") == 10, "invalid constant symbol");
    require(index.at("Compare/result").at("isVar") == true, "result must be a var-parameter");
    require(index.at("Compare/x").at("isVar") == false, "x must be a value parameter");
    const auto& signature = index.at("global/Compare").at("paramsSignature");
    require(signature == json::array({{{"type", "integer"}, {"isVar", false}},
                                     {{"type", "boolean"}, {"isVar", true}}}), "invalid signature");
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: fixture_tests <tests-directory>\n";
        return 1;
    }
    try {
        check(fs::path(argv[1]));
        std::cout << "Reference fixture checks passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "fixture_tests: " << error.what() << '\n';
        return 1;
    }
}

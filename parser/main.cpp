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

    void declareGlobal(const DeclaredName& identifier, const std::string& kind,
                       const std::string& type, const json& value)
    {
        for (const auto& symbol : symbols_)
        {
            if (symbol.at("scope").get<std::string>() == "global" &&
                nameKey(symbol.at("name").get<std::string>()) == nameKey(identifier.name))
            {
                throw std::runtime_error("line " + std::to_string(identifier.line) +
                                         ": duplicate declaration of '" + identifier.name + "'");
            }
        }
        symbols_.push_back({{"name", identifier.name}, {"kind", kind}, {"type", type},
                            {"isVar", false}, {"scope", "global"}, {"value", value}});
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

    json parseConstDeclaration()
    {
        const auto identifier = readDeclaredName();
        expect("OP", "=");
        json value;
        std::string type;
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
            type = "integer";
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
            type = "boolean";
            advance();
        }
        else
        {
            fail("expected an integer or boolean constant literal");
        }
        expect("DELIM", ";");
        declareGlobal(identifier, "const", type, value);
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
            declareGlobal(identifier, "var", type, nullptr);
            names.push_back(identifier.name);
        }
        return {{"node", "VarDecl"}, {"names", names}, {"type", type}};
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
        }
        catch (const json::parse_error& error)
        {
            throw std::runtime_error(std::string("JSON parse error: ") + error.what());
        }

        validateTokens(jsonData);

        Parser parser(jsonData);
        const std::string programName = parser.parseProgramHeader();

        const json declarations = parser.parseGlobalDeclarations();

        // Do not write output files until the entire program has been checked.
        std::cout << "Program header parsed: " << programName << '\n';
        std::cout << "Global declarations:\n" << declarations.dump(2) << '\n';
        std::cout << "Symbols:\n" << parser.symbols().dump(2) << '\n';
        std::cout << "Procedure and statement parsing is not implemented yet.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "parser: " << error.what() << '\n';
        return 1;
    }
}
#endif

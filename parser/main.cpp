#include "json.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

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

private:
    const json& tokens_;
    std::size_t pos_ = 0;

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

        // Only the header is parsed at this stage; do not write a partial AST.
        std::cout << "Program header parsed: " << programName << '\n';
        std::cout << "Block parsing is not implemented yet.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "parser: " << error.what() << '\n';
        return 1;
    }
}

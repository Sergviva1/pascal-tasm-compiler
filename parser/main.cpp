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

        // Temporary debug output; Pascal parsing will be added next.
        std::cout << "Parsed JSON data:\n" << jsonData.dump(4) << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "parser: " << error.what() << '\n';
        return 1;
    }
}

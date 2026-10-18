#include "codegen/code_generator.hpp"

#include "json.hpp"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

nlohmann::json loadJson(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open '" + path + "'");
    }

    nlohmann::json result;
    input >> result;
    return result;
}

void writeAssembly(const std::string& assembly) {
    std::ofstream output("program.asm");
    if (!output.is_open()) {
        throw std::runtime_error("cannot create 'program.asm'");
    }
    output << assembly;
    if (!output) {
        throw std::runtime_error("cannot write 'program.asm'");
    }
}

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: codegen <tree.json> <symbols.tab>\n";
        return 1;
    }

    try {
        const nlohmann::json tree = loadJson(argv[1]);
        const nlohmann::json symbols = loadJson(argv[2]);
        pascal::CodeGenerator generator(tree, symbols);
        writeAssembly(generator.generate());
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "codegen: " << error.what() << '\n';
        return 1;
    }
}

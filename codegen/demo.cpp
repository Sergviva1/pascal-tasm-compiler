#include "generator.hpp"
#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "Usage: codegen_demo <output.asm>\n";
        return 1;
    }
    Program program;
    program.variables[program.variableCount++] = "x";
    program.statements[program.statementCount++] = {"x", {NUMBER, 10, ""}};
    return generate(program, argv[1]) ? 0 : 1;
}

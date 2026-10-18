// Whitespace-separated token snippets are test fixtures, not a production lexer.
#define PARSER_NO_MAIN
#include "parser/main.cpp"
#include "codegen/code_generator.hpp"

#include <algorithm>
#include <cstdint>
#include <sstream>

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

json tokensFor(const std::string& source) {
    const std::vector<std::string> keywords = {
        "program", "var", "const", "procedure", "integer", "boolean",
        "begin", "end", "if", "then", "else", "div", "mod"};
    const std::vector<std::string> operators = {":=", "+", "-", "*", "=", "<>", "<", ">", "<=", ">="};
    const std::vector<std::string> delimiters = {";", ",", ":", "(", ")", "."};
    json result = json::array();
    std::istringstream input(source);
    for (std::string word; input >> word;) {
        std::string type = "IDENT";
        if (std::find(keywords.begin(), keywords.end(), word) != keywords.end()) type = "KEYWORD";
        else if (std::find(operators.begin(), operators.end(), word) != operators.end()) type = "OP";
        else if (std::find(delimiters.begin(), delimiters.end(), word) != delimiters.end()) type = "DELIM";
        else if (word == "true" || word == "false") type = "BOOL_LITERAL";
        else if (word.find_first_not_of("0123456789") == std::string::npos) type = "INT_LITERAL";
        result.push_back({{"type", type}, {"value", word}, {"line", 1}});
    }
    result.push_back({{"type", "EOF"}, {"value", ""}, {"line", 1}});
    return result;
}

std::string generate(const std::string& source, bool reverseSymbols = false) {
    const json tokens = tokensFor(source);
    validateTokens(tokens);
    Parser parser(tokens);
    const json tree = parser.parseProgram();
    json symbols = parser.symbols();
    if (reverseSymbols) std::reverse(symbols.begin(), symbols.end());
    pascal::CodeGenerator generator(tree, symbols);
    return generator.generate();
}

void contains(const std::string& assembly, const std::string& text) {
    check(assembly.find(text) != std::string::npos, "missing ASM: " + text);
}

void testNearPointers() {
    const std::string assembly = generate(
        "program P ; var g : integer ; "
        "procedure Set ( var p : integer ) ; begin p := p + 1 end ; "
        "procedure Forward ( var p : integer ) ; begin Set ( p ) end ; "
        "procedure Local ( value : integer ) ; var x : integer ; "
        "begin x := 1 ; Set ( x ) ; Set ( value ) ; Forward ( x ) end ; "
        "begin g := 0 ; Set ( g ) ; Local ( g ) end .");
    contains(assembly, ".model small, nearstack");
    contains(assembly, "mov bx, ss\n    sub bx, ax\n"
                       "    shl bx, 1\n    shl bx, 1\n    shl bx, 1\n    shl bx, 1\n"
                       "    cli\n    mov ss, ax\n    add sp, bx\n    sti");
    check(assembly.find("mov ss, ax") < assembly.find("push ax"), "stack must be rebased before use");
    contains(assembly, "lea ax, cg_var_");
    contains(assembly, "lea ax, [bp-2]\n    push ax");
    contains(assembly, "lea ax, [bp+4]\n    push ax");
    contains(assembly, "mov ax, [bp+4]\n    push ax");
    contains(assembly, "mov bx, [bp+4]\n    mov ax, [bx]");
    contains(assembly, "mov bx, [bp+4]\n    mov [bx], ax");
    contains(assembly, "ret 2");

    // Verify the rebasing formula for DOS stacks at different paragraph offsets.
    // All used offsets fit DGROUP's 64 KiB near-stack model.
    for (unsigned paragraphs : {0U, 1U, 17U, 128U, 2048U}) {
        constexpr unsigned dataSegment = 0x1234;
        constexpr unsigned oldSp = 0x100;
        const unsigned oldSs = dataSegment + paragraphs;
        const unsigned newSp = oldSp + paragraphs * 16;
        check(oldSs * 16 + oldSp == dataSegment * 16 + newSp, "stack physical address changed");
        for (unsigned displacement : {2U, 4U, 12U}) {
            const auto pointer = static_cast<std::uint16_t>(newSp - displacement);
            check(dataSegment * 16 + pointer == oldSs * 16 + oldSp - displacement,
                  "near pointer does not reference the original stack cell");
        }
    }
}

void testAssemblyNames() {
    const std::string source =
        "program P ; const ds = 1 ; var ax , cg_var_0 , cg_else_1 : integer ; "
        "procedure main ( var bx : integer ) ; begin bx := ds end ; "
        "procedure cg_proc_0 ( ) ; begin main ( ax ) end ; "
        "begin ax := ds ; cg_proc_0 ( ) ; "
        "if ax = 1 then cg_else_1 := ax else cg_var_0 := 2 end .";
    const std::string assembly = generate(source);
    check(assembly == generate(source, true), "ASM names depend on symbol table order");
    check(assembly.find("ax dw") == std::string::npos, "raw register name emitted");
    check(assembly.find("ds equ") == std::string::npos, "raw segment register emitted");
    check(assembly.find("call main") == std::string::npos, "user procedure called entry point");
    contains(assembly, "call cg_proc_");
    contains(assembly, "cg_else_1:");
    contains(assembly, "cg_const_0 equ 1");
    // All emitted declarations/labels occupy a single case-insensitive ASM namespace.
    std::vector<std::string> names;
    std::istringstream lines(assembly);
    for (std::string line; std::getline(lines, line);) {
        std::istringstream words(line);
        std::string name, directive;
        words >> name >> directive;
        if (!name.empty() && (name.back() == ':' || directive == "proc" || directive == "dw" || directive == "equ")) {
            if (name.back() == ':') name.pop_back();
            check(std::find(names.begin(), names.end(), name) == names.end(), "duplicate ASM name: " + name);
            names.push_back(name);
        }
    }
}

void testGlobalProcedure() {
    const std::string source =
        "program P ; const C = 1 ; var x : integer ; "
        "procedure global ( var x : integer ) ; const C = 2 ; "
        "begin x := C end ; begin x := C ; global ( x ) end .";
    const std::string assembly = generate(source);
    check(assembly == generate(source, true), "ambiguous global scope depends on table order");
    contains(assembly, "mov ax, 1\n    mov cg_var_1, ax");
    contains(assembly, "mov ax, 2\n    mov bx, [bp+4]\n    mov [bx], ax");
    contains(assembly, "lea ax, cg_var_1\n    push ax\n    call cg_proc_");
    // Identical constants in both scopes must still be consumed as distinct rows.
    generate("program P ; const C = 1 ; procedure GLOBAL ( a : integer ) ; "
             "const C = 1 ; begin a := C end ; begin GLOBAL ( C ) end .", true);
    const std::string locals = generate(
        "program P ; var x : integer ; procedure global ( ) ; var x : integer ; "
        "begin x := 2 end ; begin x := 1 ; global ( ) end .", true);
    contains(locals, "mov ax, 2\n    mov [bp-2], ax");
    contains(locals, "mov ax, 1\n    mov cg_var_0, ax");
}

} // namespace

int main() {
    try {
        testNearPointers();
        testAssemblyNames();
        testGlobalProcedure();
        std::cout << "Codegen regressions passed: near pointers, ASM names, global procedure\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "codegen_regression_tests: " << error.what() << '\n';
        return 1;
    }
}

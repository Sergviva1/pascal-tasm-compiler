#include <iostream>

int main(int argc, char*[]) {
    if (argc != 3) {
        std::cerr << "usage: codegen <tree.json> <symbols.tab>\n";
        return 1;
    }

    // TODO: Implement SPEC section 3. Do not create outputs until implemented.
    std::cerr << "codegen: not implemented yet\n";
    return 1;
}

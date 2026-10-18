#include "codegen/code_generator.hpp"

#include <stdexcept>

namespace pascal {

void CodeGenerator::generateExpression(const Json& expression) {
    const std::string node = expression.at("node").get<std::string>();
    if (node == "IntLiteral") {
        output_ << "    mov ax, " << expression.at("value").get<long long>() << "\n";
    } else if (node == "BoolLiteral") {
        output_ << "    mov ax, " << (expression.at("value").get<bool>() ? 1 : 0) << "\n";
    } else if (node == "VarRef") {
        loadVariable(expression.at("name").get<std::string>());
    } else if (node == "ConstRef") {
        const Json& symbol = findSymbol(expression.at("name").get<std::string>());
        if (symbol.at("kind") != "const") {
            throw std::runtime_error("symbol is not a constant");
        }
        output_ << "    mov ax, "
                << valueText(symbol.at("value"), symbol.at("type").get<std::string>()) << "\n";
    } else if (node == "UnaryMinus") {
        generateExpression(expression.at("operand"));
        output_ << "    neg ax\n";
    } else if (node == "BinOp") {
        generateBinaryExpression(expression);
    } else {
        throw std::runtime_error("unsupported expression node '" + node + "'");
    }
}

void CodeGenerator::generateBinaryExpression(const Json& expression) {
    generateExpression(expression.at("left"));
    output_ << "    push ax\n";
    generateExpression(expression.at("right"));
    output_ << "    mov cx, ax\n"
            << "    pop ax\n";

    const std::string operation = expression.at("op").get<std::string>();
    if (operation == "+") {
        output_ << "    add ax, cx\n";
    } else if (operation == "-") {
        output_ << "    sub ax, cx\n";
    } else if (operation == "*") {
        output_ << "    imul cx\n";
    } else if (operation == "div" || operation == "mod") {
        output_ << "    cwd\n"
                << "    idiv cx\n";
        if (operation == "mod") {
            output_ << "    mov ax, dx\n";
        }
    } else {
        throw std::runtime_error("unsupported binary operator '" + operation + "'");
    }
}

void CodeGenerator::loadVariable(const std::string& name) {
    const Json& symbol = findSymbol(name);
    const std::string kind = symbol.at("kind").get<std::string>();
    if (kind == "const" || kind == "proc") {
        throw std::runtime_error("symbol '" + name + "' is not a variable");
    }

    if (isGlobal(symbol)) {
        output_ << "    mov ax, " << asmName(symbol) << "\n";
        return;
    }

    const std::string memory = memoryAtBp(stackOffset(name));
    if (kind == "param" && symbol.at("isVar").get<bool>()) {
        output_ << "    mov bx, " << memory << "\n"
                << "    mov ax, [bx]\n";
    } else {
        output_ << "    mov ax, " << memory << "\n";
    }
}

void CodeGenerator::storeVariable(const std::string& name) {
    const Json& symbol = findSymbol(name);
    const std::string kind = symbol.at("kind").get<std::string>();
    if (kind == "const" || kind == "proc") {
        throw std::runtime_error("cannot assign to '" + name + "'");
    }

    if (isGlobal(symbol)) {
        output_ << "    mov " << asmName(symbol) << ", ax\n";
        return;
    }

    const std::string memory = memoryAtBp(stackOffset(name));
    if (kind == "param" && symbol.at("isVar").get<bool>()) {
        output_ << "    mov bx, " << memory << "\n"
                << "    mov [bx], ax\n";
    } else {
        output_ << "    mov " << memory << ", ax\n";
    }
}

void CodeGenerator::loadAddress(const std::string& name) {
    const Json& symbol = findSymbol(name);
    const std::string kind = symbol.at("kind").get<std::string>();
    if (kind == "const" || kind == "proc") {
        throw std::runtime_error("cannot take address of '" + name + "'");
    }

    if (isGlobal(symbol)) {
        output_ << "    lea ax, " << asmName(symbol) << "\n";
        return;
    }

    const std::string memory = memoryAtBp(stackOffset(name));
    if (kind == "param" && symbol.at("isVar").get<bool>()) {
        output_ << "    mov ax, " << memory << "\n";
    } else {
        output_ << "    lea ax, " << memory << "\n";
    }
}

} // namespace pascal

#include "codegen/code_generator.hpp"

#include <stdexcept>

namespace pascal {

void CodeGenerator::generateStatement(const Json& statement) {
    const std::string node = statement.at("node").get<std::string>();
    if (node == "Assign") {
        generateExpression(statement.at("expr"));
        storeVariable(statement.at("target").get<std::string>());
    } else if (node == "If") {
        generateIf(statement);
    } else if (node == "ProcCall") {
        generateCall(statement);
    } else if (node == "Compound") {
        for (const auto& child : statement.at("statements")) {
            generateStatement(child);
        }
    } else {
        throw std::runtime_error("unsupported statement node '" + node + "'");
    }
}

void CodeGenerator::generateIf(const Json& statement) {
    const Json& condition = statement.at("condition");
    requireNode(condition, "Compare");

    generateExpression(condition.at("left"));
    output_ << "    push ax\n";
    generateExpression(condition.at("right"));
    output_ << "    mov bx, ax\n"
            << "    pop ax\n"
            << "    cmp ax, bx\n";

    const int label = ++labelCounter_;
    const std::string endLabel = "cg_endif_" + std::to_string(label);
    const bool hasElse = !statement.at("elseBranch").is_null();
    const std::string falseLabel = hasElse ? "cg_else_" + std::to_string(label) : endLabel;
    const std::string operation = condition.at("op").get<std::string>();

    static const std::unordered_map<std::string, std::string> inverseJumps = {
        {"=", "jne"}, {"<>", "je"}, {"<", "jge"},
        {">", "jle"}, {"<=", "jg"}, {">=", "jl"}
    };
    const auto jump = inverseJumps.find(operation);
    if (jump == inverseJumps.end()) {
        throw std::runtime_error("unsupported comparison operator '" + operation + "'");
    }

    output_ << "    " << jump->second << " " << falseLabel << "\n";
    generateStatement(statement.at("thenBranch"));
    if (hasElse) {
        output_ << "    jmp " << endLabel << "\n"
                << falseLabel << ":\n";
        generateStatement(statement.at("elseBranch"));
    }
    output_ << endLabel << ":\n";
}

void CodeGenerator::generateCall(const Json& call) {
    const std::string requestedName = call.at("name").get<std::string>();
    const auto procedureIt = symbolIndex_.find(symbolKey("", requestedName));
    if (procedureIt == symbolIndex_.end() || procedureIt->second->at("kind") != "proc") {
        throw std::runtime_error("unknown procedure '" + requestedName + "'");
    }

    const Json& procedure = *procedureIt->second;
    const Json& signature = procedure.at("paramsSignature");
    const Json& arguments = call.at("args");
    if (signature.size() != arguments.size()) {
        throw std::runtime_error("invalid argument count in call to '" + requestedName + "'");
    }

    for (std::size_t index = arguments.size(); index-- > 0;) {
        if (signature.at(index).at("isVar").get<bool>()) {
            const Json& argument = arguments.at(index);
            requireNode(argument, "VarRef");
            loadAddress(argument.at("name").get<std::string>());
        } else {
            generateExpression(arguments.at(index));
        }
        output_ << "    push ax\n";
    }
    output_ << "    call " << asmName(procedure) << "\n";
}

} // namespace pascal

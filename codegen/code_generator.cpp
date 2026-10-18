#include "codegen/code_generator.hpp"

#include <cctype>
#include <stdexcept>
#include <utility>

namespace pascal {

CodeGenerator::CodeGenerator(const Json& tree, const Json& symbols)
    : tree_(tree), symbols_(symbols) {
    indexSymbols();
    indexProcedureFrames();
}

std::string CodeGenerator::generate() {
    requireNode(tree_, "Program");
    const Json& block = tree_.at("block");
    requireNode(block, "Block");

    output_ << ".model small\n"
            << ".stack 100h\n\n"
            << ".data\n";
    generateData(block);

    output_ << "\n.code\n"
            << "main proc\n"
            << "    mov ax, @data\n"
            << "    mov ds, ax\n\n";

    currentScope_ = "global";
    for (const auto& statement : block.at("body")) {
        generateStatement(statement);
    }

    output_ << "\n    mov ax, 4c00h\n"
            << "    int 21h\n"
            << "main endp\n";

    for (const auto& procedure : block.at("procs")) {
        generateProcedure(procedure);
    }

    output_ << "\nend main\n";
    return output_.str();
}

std::string CodeGenerator::lower(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string CodeGenerator::symbolKey(const std::string& scope, const std::string& name) {
    return lower(scope) + "\n" + lower(name);
}

void CodeGenerator::requireNode(const Json& node, const std::string& expected) {
    if (!node.is_object() || !node.contains("node") || node.at("node") != expected) {
        throw std::runtime_error("expected AST node '" + expected + "'");
    }
}

void CodeGenerator::indexSymbols() {
    if (!symbols_.is_array()) {
        throw std::runtime_error("symbol table must be a JSON array");
    }

    for (const auto& symbol : symbols_) {
        const std::string name = symbol.at("name").get<std::string>();
        const std::string scope = symbol.at("scope").get<std::string>();
        const std::string key = symbolKey(scope, name);
        if (!symbolIndex_.emplace(key, &symbol).second) {
            throw std::runtime_error("duplicate symbol '" + name + "' in scope '" + scope + "'");
        }
    }
}

void CodeGenerator::indexProcedureFrames() {
    requireNode(tree_, "Program");
    const Json& rootBlock = tree_.at("block");
    requireNode(rootBlock, "Block");

    for (const auto& procedure : rootBlock.at("procs")) {
        requireNode(procedure, "ProcDecl");
        const std::string procedureName = procedure.at("name").get<std::string>();
        ProcedureFrame frame;

        int parameterOffset = 4;
        for (const auto& parameter : procedure.at("params")) {
            requireNode(parameter, "Param");
            for (const auto& nameValue : parameter.at("names")) {
                const std::string name = nameValue.get<std::string>();
                if (!frame.slots.emplace(lower(name), StackSlot{parameterOffset}).second) {
                    throw std::runtime_error("duplicate stack slot '" + name + "'");
                }
                parameterOffset += 2;
            }
        }
        frame.parameterBytes = parameterOffset - 4;

        int localOffset = -2;
        const Json& procedureBlock = procedure.at("block");
        requireNode(procedureBlock, "Block");
        for (const auto& declaration : procedureBlock.at("vars")) {
            requireNode(declaration, "VarDecl");
            for (const auto& nameValue : declaration.at("names")) {
                const std::string name = nameValue.get<std::string>();
                if (!frame.slots.emplace(lower(name), StackSlot{localOffset}).second) {
                    throw std::runtime_error("duplicate stack slot '" + name + "'");
                }
                localOffset -= 2;
                frame.localBytes += 2;
            }
        }

        if (!frames_.emplace(lower(procedureName), std::move(frame)).second) {
            throw std::runtime_error("duplicate procedure '" + procedureName + "'");
        }
    }
}

const CodeGenerator::Json& CodeGenerator::findSymbol(const std::string& name) const {
    if (currentScope_ != "global") {
        const auto local = symbolIndex_.find(symbolKey(currentScope_, name));
        if (local != symbolIndex_.end()) {
            return *local->second;
        }
    }

    const auto global = symbolIndex_.find(symbolKey("global", name));
    if (global != symbolIndex_.end()) {
        return *global->second;
    }
    throw std::runtime_error("unknown symbol '" + name + "'");
}

const CodeGenerator::ProcedureFrame& CodeGenerator::currentFrame() const {
    const auto frame = frames_.find(lower(currentScope_));
    if (frame == frames_.end()) {
        throw std::runtime_error("missing stack frame for procedure '" + currentScope_ + "'");
    }
    return frame->second;
}

int CodeGenerator::stackOffset(const std::string& name) const {
    const auto slot = currentFrame().slots.find(lower(name));
    if (slot == currentFrame().slots.end()) {
        throw std::runtime_error("missing stack slot for '" + name + "'");
    }
    return slot->second.offset;
}

std::string CodeGenerator::memoryAtBp(int offset) {
    if (offset >= 0) {
        return "[bp+" + std::to_string(offset) + "]";
    }
    return "[bp" + std::to_string(offset) + "]";
}

std::string CodeGenerator::valueText(const Json& value, const std::string& type) {
    if (type == "boolean") {
        if (!value.is_boolean()) {
            throw std::runtime_error("boolean constant must contain a JSON boolean");
        }
        return value.get<bool>() ? "1" : "0";
    }
    if (type == "integer") {
        if (!value.is_number_integer()) {
            throw std::runtime_error("integer constant must contain a JSON integer");
        }
        return std::to_string(value.get<long long>());
    }
    throw std::runtime_error("unsupported Pascal type '" + type + "'");
}

void CodeGenerator::generateData(const Json& block) {
    for (const auto& constant : block.at("consts")) {
        requireNode(constant, "ConstDecl");
        output_ << "    " << constant.at("name").get<std::string>() << " equ "
                << valueText(constant.at("value"), constant.at("type").get<std::string>()) << '\n';
    }
    for (const auto& declaration : block.at("vars")) {
        requireNode(declaration, "VarDecl");
        for (const auto& name : declaration.at("names")) {
            output_ << "    " << name.get<std::string>() << " dw ?\n";
        }
    }
}

void CodeGenerator::generateProcedure(const Json& procedure) {
    requireNode(procedure, "ProcDecl");
    const std::string name = procedure.at("name").get<std::string>();
    const auto frameIt = frames_.find(lower(name));
    if (frameIt == frames_.end()) {
        throw std::runtime_error("missing frame for procedure '" + name + "'");
    }
    const ProcedureFrame& frame = frameIt->second;

    const Json& block = procedure.at("block");
    requireNode(block, "Block");
    if (!block.at("procs").empty()) {
        throw std::runtime_error("nested procedures are not supported");
    }

    output_ << "\n" << name << " proc\n"
            << "    push bp\n"
            << "    mov bp, sp\n";
    if (frame.localBytes != 0) {
        output_ << "    sub sp, " << frame.localBytes << "\n";
    }

    const std::string previousScope = currentScope_;
    currentScope_ = name;
    for (const auto& statement : block.at("body")) {
        generateStatement(statement);
    }
    currentScope_ = previousScope;

    if (frame.localBytes != 0) {
        output_ << "    mov sp, bp\n";
    }
    output_ << "    pop bp\n";
    if (frame.parameterBytes == 0) {
        output_ << "    ret\n";
    } else {
        output_ << "    ret " << frame.parameterBytes << "\n";
    }
    output_ << name << " endp\n";
}

} // namespace pascal

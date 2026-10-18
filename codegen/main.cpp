#include "json.hpp"

#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

using json = nlohmann::json;

namespace {

std::string lower(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string symbolKey(const std::string& scope, const std::string& name) {
    return lower(scope) + "\n" + lower(name);
}

json loadJson(const std::string& path) {
    std::ifstream input(path);
    if (!input.is_open()) {
        throw std::runtime_error("cannot open '" + path + "'");
    }

    json result;
    input >> result;
    return result;
}

class CodeGenerator {
public:
    CodeGenerator(const json& tree, const json& symbols)
        : tree_(tree), symbols_(symbols) {
        indexSymbols();
        indexProcedureFrames();
    }

    std::string generate() {
        requireNode(tree_, "Program");
        const json& block = tree_.at("block");
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

private:
    struct StackSlot {
        int offset;
    };

    struct ProcedureFrame {
        std::unordered_map<std::string, StackSlot> slots;
        int parameterBytes = 0;
        int localBytes = 0;
    };

    const json& tree_;
    const json& symbols_;
    std::unordered_map<std::string, const json*> symbolIndex_;
    std::unordered_map<std::string, ProcedureFrame> frames_;
    std::ostringstream output_;
    std::string currentScope_ = "global";
    int labelCounter_ = 0;

    static void requireNode(const json& node, const std::string& expected) {
        if (!node.is_object() || !node.contains("node") || node.at("node") != expected) {
            throw std::runtime_error("expected AST node '" + expected + "'");
        }
    }

    void indexSymbols() {
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

    void indexProcedureFrames() {
        requireNode(tree_, "Program");
        const json& rootBlock = tree_.at("block");
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
            const json& procedureBlock = procedure.at("block");
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

    const json& findSymbol(const std::string& name) const {
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

    const ProcedureFrame& currentFrame() const {
        const auto frame = frames_.find(lower(currentScope_));
        if (frame == frames_.end()) {
            throw std::runtime_error("missing stack frame for procedure '" + currentScope_ + "'");
        }
        return frame->second;
    }

    int stackOffset(const std::string& name) const {
        const auto slot = currentFrame().slots.find(lower(name));
        if (slot == currentFrame().slots.end()) {
            throw std::runtime_error("missing stack slot for '" + name + "'");
        }
        return slot->second.offset;
    }

    static std::string memoryAtBp(int offset) {
        if (offset >= 0) {
            return "[bp+" + std::to_string(offset) + "]";
        }
        return "[bp" + std::to_string(offset) + "]";
    }

    static std::string valueText(const json& value, const std::string& type) {
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

    void generateData(const json& block) {
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

    void generateProcedure(const json& procedure) {
        requireNode(procedure, "ProcDecl");
        const std::string name = procedure.at("name").get<std::string>();
        const auto frameIt = frames_.find(lower(name));
        if (frameIt == frames_.end()) {
            throw std::runtime_error("missing frame for procedure '" + name + "'");
        }
        const ProcedureFrame& frame = frameIt->second;

        const json& block = procedure.at("block");
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

    void generateStatement(const json& statement) {
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

    void generateIf(const json& statement) {
        const json& condition = statement.at("condition");
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

    void generateCall(const json& call) {
        const std::string requestedName = call.at("name").get<std::string>();
        const auto procedureIt = symbolIndex_.find(symbolKey("global", requestedName));
        if (procedureIt == symbolIndex_.end() || procedureIt->second->at("kind") != "proc") {
            throw std::runtime_error("unknown procedure '" + requestedName + "'");
        }

        const json& procedure = *procedureIt->second;
        const json& signature = procedure.at("paramsSignature");
        const json& arguments = call.at("args");
        if (signature.size() != arguments.size()) {
            throw std::runtime_error("invalid argument count in call to '" + requestedName + "'");
        }

        for (std::size_t index = arguments.size(); index-- > 0;) {
            if (signature.at(index).at("isVar").get<bool>()) {
                const json& argument = arguments.at(index);
                requireNode(argument, "VarRef");
                loadAddress(argument.at("name").get<std::string>());
            } else {
                generateExpression(arguments.at(index));
            }
            output_ << "    push ax\n";
        }
        output_ << "    call " << procedure.at("name").get<std::string>() << "\n";
    }

    void generateExpression(const json& expression) {
        const std::string node = expression.at("node").get<std::string>();
        if (node == "IntLiteral") {
            output_ << "    mov ax, " << expression.at("value").get<long long>() << "\n";
        } else if (node == "BoolLiteral") {
            output_ << "    mov ax, " << (expression.at("value").get<bool>() ? 1 : 0) << "\n";
        } else if (node == "VarRef") {
            loadVariable(expression.at("name").get<std::string>());
        } else if (node == "ConstRef") {
            const json& symbol = findSymbol(expression.at("name").get<std::string>());
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

    void generateBinaryExpression(const json& expression) {
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

    void loadVariable(const std::string& name) {
        const json& symbol = findSymbol(name);
        const std::string kind = symbol.at("kind").get<std::string>();
        if (kind == "const" || kind == "proc") {
            throw std::runtime_error("symbol '" + name + "' is not a variable");
        }

        if (symbol.at("scope") == "global") {
            output_ << "    mov ax, " << symbol.at("name").get<std::string>() << "\n";
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

    void storeVariable(const std::string& name) {
        const json& symbol = findSymbol(name);
        const std::string kind = symbol.at("kind").get<std::string>();
        if (kind == "const" || kind == "proc") {
            throw std::runtime_error("cannot assign to '" + name + "'");
        }

        if (symbol.at("scope") == "global") {
            output_ << "    mov " << symbol.at("name").get<std::string>() << ", ax\n";
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

    void loadAddress(const std::string& name) {
        const json& symbol = findSymbol(name);
        const std::string kind = symbol.at("kind").get<std::string>();
        if (kind == "const" || kind == "proc") {
            throw std::runtime_error("cannot take address of '" + name + "'");
        }

        if (symbol.at("scope") == "global") {
            output_ << "    lea ax, " << symbol.at("name").get<std::string>() << "\n";
            return;
        }

        const std::string memory = memoryAtBp(stackOffset(name));
        if (kind == "param" && symbol.at("isVar").get<bool>()) {
            output_ << "    mov ax, " << memory << "\n";
        } else {
            output_ << "    lea ax, " << memory << "\n";
        }
    }
};

} // namespace

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "usage: codegen <tree.json> <symbols.tab>\n";
        return 1;
    }

    try {
        const json tree = loadJson(argv[1]);
        const json symbols = loadJson(argv[2]);
        CodeGenerator generator(tree, symbols);
        const std::string assembly = generator.generate();

        std::ofstream output("program.asm");
        if (!output.is_open()) {
            throw std::runtime_error("cannot create 'program.asm'");
        }
        output << assembly;
        if (!output) {
            throw std::runtime_error("cannot write 'program.asm'");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "codegen: " << error.what() << '\n';
        return 1;
    }
}

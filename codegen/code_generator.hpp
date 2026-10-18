#pragma once

#include "json.hpp"

#include <sstream>
#include <string>
#include <unordered_map>

namespace pascal {

class CodeGenerator {
public:
    using Json = nlohmann::json;

    CodeGenerator(const Json& tree, const Json& symbols);
    std::string generate();

private:
    struct StackSlot {
        int offset;
    };

    struct ProcedureFrame {
        std::unordered_map<std::string, StackSlot> slots;
        int parameterBytes = 0;
        int localBytes = 0;
    };

    const Json& tree_;
    const Json& symbols_;
    std::unordered_map<std::string, const Json*> symbolIndex_;
    std::unordered_map<std::string, ProcedureFrame> frames_;
    std::ostringstream output_;
    std::string currentScope_ = "global";
    int labelCounter_ = 0;

    static std::string lower(std::string value);
    static std::string symbolKey(const std::string& scope, const std::string& name);
    static void requireNode(const Json& node, const std::string& expected);
    static std::string memoryAtBp(int offset);
    static std::string valueText(const Json& value, const std::string& type);

    void indexSymbols();
    void indexProcedureFrames();
    const Json& findSymbol(const std::string& name) const;
    const ProcedureFrame& currentFrame() const;
    int stackOffset(const std::string& name) const;

    void generateData(const Json& block);
    void generateProcedure(const Json& procedure);

    void generateStatement(const Json& statement);
    void generateIf(const Json& statement);
    void generateCall(const Json& call);

    void generateExpression(const Json& expression);
    void generateBinaryExpression(const Json& expression);
    void loadVariable(const std::string& name);
    void storeVariable(const std::string& name);
    void loadAddress(const std::string& name);
};

} // namespace pascal

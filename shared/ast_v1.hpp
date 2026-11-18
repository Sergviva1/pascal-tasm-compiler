#pragma once
#include <string>

inline constexpr int CAPACITY = 100;
enum ExpressionKind { NUMBER, VARIABLE };

struct Expression {
    ExpressionKind kind = NUMBER;
    int number = 0;
    std::string name;
};
struct Assignment {
    std::string target;
    Expression expression;
};
struct Program {
    std::string variables[CAPACITY];
    int variableCount = 0;
    Assignment statements[CAPACITY]{};
    int statementCount = 0;
};

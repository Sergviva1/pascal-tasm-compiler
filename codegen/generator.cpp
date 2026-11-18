#include "generator.hpp"
#include <fstream>
#include <iostream>
#include <string>

using namespace std;

namespace {

int findVariable(const Program& program, const string& name)
{
    for (int i = 0; i < program.variableCount; ++i)
        if (program.variables[i] == name)
            return i;
    return -1;
}

const char* validate(const Program& program)
{
    if (program.variableCount < 0 || program.variableCount > CAPACITY ||
        program.statementCount < 0 || program.statementCount > CAPACITY)
        return "invalid AST size";

    for (int i = 0; i < program.variableCount; ++i)
    {
        const string& name = program.variables[i];
        if (name.empty() || findVariable(program, name) != i)
            return "empty or duplicate variable declaration";
    }

    for (int i = 0; i < program.statementCount; ++i)
    {
        const Assignment& statement = program.statements[i];
        const Expression& expression = statement.expression;

        if (findVariable(program, statement.target) == -1)
            return "missing or unknown assignment target";

        switch (expression.kind)
        {
        case NUMBER:
            if (expression.number < -32768 || expression.number > 32767)
                return "integer literal is outside the signed 16-bit range";
            break;
        case VARIABLE:
            if (expression.name.empty())
                return "VARIABLE node has no name";
            if (findVariable(program, expression.name) == -1)
                return "unknown source variable";
            break;
        default:
            return "unsupported expression kind";
        }
    }
    return nullptr;
}

} // namespace

bool generate(const Program& program, const char* path)
{
    if (const char* error = validate(program))
    {
        cerr << "Error: " << error << '\n';
        return false;
    }

    ofstream out(path);
    if (!out)
    {
        cerr << "Error: cannot open output file: " << path << '\n';
        return false;
    }

    out << ".MODEL SMALL\n"
           ".STACK 100h\n\n"
           ".DATA\n"
           "    cg_dummy DB 0\n";

    for (int i = 0; i < program.variableCount; ++i)
        out << "    v_" << i << " DW ?\n";

    out << "\n.CODE\n"
           "main PROC\n"
           "    mov ax, @data\n"
           "    mov ds, ax\n\n";

    for (int i = 0; i < program.statementCount; ++i)
    {
        const Assignment& statement = program.statements[i];
        const Expression& expression = statement.expression;
        if (expression.kind == NUMBER)
            out << "    mov ax, " << expression.number << '\n';
        else
            out << "    mov ax, v_" << findVariable(program, expression.name) << '\n';
        out << "    mov v_" << findVariable(program, statement.target) << ", ax\n\n";
    }

    out << "    mov ax, 4C00h\n"
           "    int 21h\n"
           "main ENDP\n"
           "END main\n";

    out.close();
    if (!out)
    {
        cerr << "Error: cannot write output file: " << path << '\n';
        return false;
    }

    return true;
}

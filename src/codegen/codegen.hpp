#pragma once

#include "../ast/ast.hpp"
#include "../semantics/symbol_table.hpp"
#include <string>
#include <sstream>
#include <vector>
#include <memory>

namespace mcxx {

class CodeGenerator {
public:
    CodeGenerator(SymbolTable& symTable);

    std::string generate(Program& program);

private:
    void emitLine(const std::string& line);
    void emitLabel(const std::string& label);
    std::string newLabel(const std::string& prefix = "L");
    std::string addStringLiteral(const std::string& str);

    void genGlobalVar(GlobalVarDecl& gv);
    void genClass(ClassDecl& cd);
    void genFunction(FunctionDecl& fn);
    void genBlock(BlockStmt& block);
    void genStmt(Stmt& stmt);

    // Expressions
    void genExpr(Expr& expr);
    void genLValue(Expr& expr); // computes address of lvalue and leaves in %edx

    SymbolTable& symTable;
    std::ostringstream textOut;
    std::ostringstream rdataOut;
    std::ostringstream dataOut;

    int labelCounter = 0;
    int stringLiteralCounter = 0;
    std::string currentClassName;
    int currentFunctionFrameSize = 0;
    std::string currentBreakLabel;
    std::string currentContinueLabel;
};

} // namespace mcxx

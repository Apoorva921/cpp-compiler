#pragma once

#include "../ast/ast.hpp"
#include "symbol_table.hpp"
#include <string>
#include <memory>
#include <vector>

namespace mcxx {

class TypeChecker {
public:
    TypeChecker();

    bool analyze(Program& program);
    SymbolTable& getSymbolTable() { return symTable; }
    int getErrorsCount() const { return errorCount; }

private:
    void registerBuiltins();
    std::shared_ptr<Type> resolveType(const ParsedType& pt, const SourceLocation& loc);

    void firstPassCollectClasses(Program& program);
    void firstPassCollectFunctions(Program& program);
    void analyzeFunction(FunctionDecl& fn);
    void analyzeBlock(BlockStmt& block);
    void analyzeStmt(Stmt& stmt);
    std::shared_ptr<Type> analyzeExpr(Expr& expr);

    void error(const std::string& msg, const SourceLocation& loc);

    SymbolTable symTable;
    int errorCount = 0;
    std::shared_ptr<Type> currentFunctionReturnType = nullptr;
    std::string currentClassName;
    int currentStackOffset = 0; // negative offset for local variables
};

} // namespace mcxx

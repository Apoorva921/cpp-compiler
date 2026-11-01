#pragma once

#include "../lexer/token.hpp"
#include "../ast/ast.hpp"
#include <vector>
#include <memory>
#include <string>

namespace mcxx {

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    std::unique_ptr<Program> parseProgram();

private:
    const Token& peek(int offset = 0) const;
    const Token& advance();
    bool match(TokenKind kind);
    bool check(TokenKind kind) const;
    const Token& expect(TokenKind kind, const std::string& errMsg);
    bool isAtEnd() const;

    // Declarations
    std::unique_ptr<Decl> parseTopLevelDecl();
    std::unique_ptr<ClassDecl> parseClassOrStructDecl();
    std::unique_ptr<FunctionDecl> parseExternDecl();
    std::unique_ptr<Decl> parseFunctionOrGlobalVar();

    // Type parsing
    ParsedType parseType();
    bool isTypeSpecifier(TokenKind kind) const;

    // Statements
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<BlockStmt> parseBlockStatement();
    std::unique_ptr<Stmt> parseVarDeclStatement();
    std::unique_ptr<Stmt> parseIfStatement();
    std::unique_ptr<Stmt> parseWhileStatement();
    std::unique_ptr<Stmt> parseForStatement();
    std::unique_ptr<Stmt> parseReturnStatement();

    // Expressions (Precedence climbing)
    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseAssignment();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseBitwiseOr();
    std::unique_ptr<Expr> parseBitwiseXor();
    std::unique_ptr<Expr> parseBitwiseAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseRelational();
    std::unique_ptr<Expr> parseShift();
    std::unique_ptr<Expr> parseAdditive();
    std::unique_ptr<Expr> parseMultiplicative();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parsePostfix();
    std::unique_ptr<Expr> parsePrimary();

    void error(const std::string& msg, const SourceLocation& loc);

    std::vector<Token> tokens;
    size_t cursor = 0;
};

} // namespace mcxx

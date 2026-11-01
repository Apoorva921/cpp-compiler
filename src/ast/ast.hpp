#pragma once

#include "../lexer/token.hpp"
#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace mcxx {

// Forward declarations
struct ASTVisitor;
class Type; // Semantic type pointer

enum class BaseTypeKind {
    Int,
    Char,
    Bool,
    Void,
    UserDefined // struct/class name
};

struct ParsedType {
    BaseTypeKind baseKind = BaseTypeKind::Int;
    std::string name; // for UserDefined
    int pointerDepth = 0;
    bool isArray = false;
    int arraySize = 0;

    std::string toString() const {
        std::string s;
        switch (baseKind) {
            case BaseTypeKind::Int: s = "int"; break;
            case BaseTypeKind::Char: s = "char"; break;
            case BaseTypeKind::Bool: s = "bool"; break;
            case BaseTypeKind::Void: s = "void"; break;
            case BaseTypeKind::UserDefined: s = name; break;
        }
        for (int i = 0; i < pointerDepth; ++i) s += "*";
        if (isArray) s += "[" + std::to_string(arraySize) + "]";
        return s;
    }
};

// Base AST Node
struct ASTNode {
    SourceLocation loc;
    virtual ~ASTNode() = default;
    virtual void dump(int indent = 0) const = 0;
};

// Expressions
struct Expr : public ASTNode {
    std::shared_ptr<Type> resolvedType; // populated during semantic analysis
};

struct IntLiteralExpr : public Expr {
    int value;
    IntLiteralExpr(int val, SourceLocation l) : value(val) { loc = l; }
    void dump(int indent = 0) const override;
};

struct CharLiteralExpr : public Expr {
    char value;
    CharLiteralExpr(char val, SourceLocation l) : value(val) { loc = l; }
    void dump(int indent = 0) const override;
};

struct StringLiteralExpr : public Expr {
    std::string value;
    StringLiteralExpr(std::string val, SourceLocation l) : value(std::move(val)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct BoolLiteralExpr : public Expr {
    bool value;
    BoolLiteralExpr(bool val, SourceLocation l) : value(val) { loc = l; }
    void dump(int indent = 0) const override;
};

struct NullptrExpr : public Expr {
    NullptrExpr(SourceLocation l) { loc = l; }
    void dump(int indent = 0) const override;
};

enum class VarKind {
    Local,
    Param,
    Global,
    Field
};

struct VariableExpr : public Expr {
    std::string name;
    VarKind varKind = VarKind::Local;
    int stackOffset = 0;
    std::string globalLabel;
    int fieldOffset = 0;

    VariableExpr(std::string n, SourceLocation l) : name(std::move(n)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct ThisExpr : public Expr {
    ThisExpr(SourceLocation l) { loc = l; }
    void dump(int indent = 0) const override;
};

enum class UnaryOp {
    Plus,
    Minus,
    Not,
    BitNot,
    AddressOf,
    Deref,
    PreInc,
    PreDec,
    PostInc,
    PostDec
};

struct UnaryExpr : public Expr {
    UnaryOp op;
    std::unique_ptr<Expr> operand;
    UnaryExpr(UnaryOp op, std::unique_ptr<Expr> operand, SourceLocation l)
        : op(op), operand(std::move(operand)) { loc = l; }
    void dump(int indent = 0) const override;
};

enum class BinaryOp {
    Add, Sub, Mul, Div, Mod,
    BitAnd, BitOr, BitXor, Shl, Shr,
    Eq, Ne, Lt, Le, Gt, Ge,
    LogAnd, LogOr
};

struct BinaryExpr : public Expr {
    BinaryOp op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
    BinaryExpr(BinaryOp op, std::unique_ptr<Expr> left, std::unique_ptr<Expr> right, SourceLocation l)
        : op(op), left(std::move(left)), right(std::move(right)) { loc = l; }
    void dump(int indent = 0) const override;
};

enum class AssignOp {
    Assign,
    AddAssign,
    SubAssign,
    MulAssign,
    DivAssign
};

struct AssignExpr : public Expr {
    AssignOp op;
    std::unique_ptr<Expr> target;
    std::unique_ptr<Expr> value;
    AssignExpr(AssignOp op, std::unique_ptr<Expr> target, std::unique_ptr<Expr> value, SourceLocation l)
        : op(op), target(std::move(target)), value(std::move(value)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct CallExpr : public Expr {
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::string callee, std::vector<std::unique_ptr<Expr>> args, SourceLocation l)
        : callee(std::move(callee)), args(std::move(args)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct MemberExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::string memberName;
    bool isArrow; // true for ->, false for .
    MemberExpr(std::unique_ptr<Expr> obj, std::string member, bool arrow, SourceLocation l)
        : object(std::move(obj)), memberName(std::move(member)), isArrow(arrow) { loc = l; }
    void dump(int indent = 0) const override;
};

struct MethodCallExpr : public Expr {
    std::unique_ptr<Expr> object;
    std::string methodName;
    bool isArrow;
    std::vector<std::unique_ptr<Expr>> args;
    MethodCallExpr(std::unique_ptr<Expr> obj, std::string method, bool arrow,
                   std::vector<std::unique_ptr<Expr>> args, SourceLocation l)
        : object(std::move(obj)), methodName(std::move(method)), isArrow(arrow), args(std::move(args)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct IndexExpr : public Expr {
    std::unique_ptr<Expr> array;
    std::unique_ptr<Expr> index;
    IndexExpr(std::unique_ptr<Expr> arr, std::unique_ptr<Expr> idx, SourceLocation l)
        : array(std::move(arr)), index(std::move(idx)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct NewExpr : public Expr {
    ParsedType allocatedType;
    std::vector<std::unique_ptr<Expr>> constructorArgs;
    std::unique_ptr<Expr> arraySizeExpr; // if new T[n]
    NewExpr(ParsedType type, std::vector<std::unique_ptr<Expr>> args,
            std::unique_ptr<Expr> arraySize, SourceLocation l)
        : allocatedType(std::move(type)), constructorArgs(std::move(args)),
          arraySizeExpr(std::move(arraySize)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct SizeofExpr : public Expr {
    bool isType;
    ParsedType targetType;
    std::unique_ptr<Expr> targetExpr;
    SizeofExpr(ParsedType t, SourceLocation l)
        : isType(true), targetType(std::move(t)), targetExpr(nullptr) { loc = l; }
    SizeofExpr(std::unique_ptr<Expr> e, SourceLocation l)
        : isType(false), targetExpr(std::move(e)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct EndlExpr : public Expr {
    EndlExpr(SourceLocation l) { loc = l; }
    void dump(int indent = 0) const override;
};

// Statements
struct Stmt : public ASTNode {};

struct CoutStmt : public Stmt {
    std::vector<std::unique_ptr<Expr>> expressions;
    CoutStmt(std::vector<std::unique_ptr<Expr>> exprs, SourceLocation l)
        : expressions(std::move(exprs)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct CinStmt : public Stmt {
    std::vector<std::unique_ptr<Expr>> targets;
    CinStmt(std::vector<std::unique_ptr<Expr>> t, SourceLocation l)
        : targets(std::move(t)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct BlockStmt : public Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
    BlockStmt(std::vector<std::unique_ptr<Stmt>> stmts, SourceLocation l)
        : statements(std::move(stmts)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct ExprStmt : public Stmt {
    std::unique_ptr<Expr> expr;
    ExprStmt(std::unique_ptr<Expr> e, SourceLocation l) : expr(std::move(e)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct VarDeclStmt : public Stmt {
    ParsedType type;
    std::string name;
    std::unique_ptr<Expr> initializer;
    int stackOffset = 0;

    VarDeclStmt(ParsedType t, std::string n, std::unique_ptr<Expr> init, SourceLocation l)
        : type(std::move(t)), name(std::move(n)), initializer(std::move(init)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct DeclGroupStmt : public Stmt {
    std::vector<std::unique_ptr<VarDeclStmt>> decls;
    DeclGroupStmt(std::vector<std::unique_ptr<VarDeclStmt>> d, SourceLocation l)
        : decls(std::move(d)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct IfStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> thenBranch;
    std::unique_ptr<Stmt> elseBranch; // may be nullptr
    IfStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> thenB,
           std::unique_ptr<Stmt> elseB, SourceLocation l)
        : condition(std::move(cond)), thenBranch(std::move(thenB)),
          elseBranch(std::move(elseB)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct WhileStmt : public Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Stmt> body;
    WhileStmt(std::unique_ptr<Expr> cond, std::unique_ptr<Stmt> b, SourceLocation l)
        : condition(std::move(cond)), body(std::move(b)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct ForStmt : public Stmt {
    std::unique_ptr<Stmt> init; // var decl or expr stmt
    std::unique_ptr<Expr> condition;
    std::unique_ptr<Expr> step;
    std::unique_ptr<Stmt> body;
    ForStmt(std::unique_ptr<Stmt> init, std::unique_ptr<Expr> cond,
            std::unique_ptr<Expr> step, std::unique_ptr<Stmt> b, SourceLocation l)
        : init(std::move(init)), condition(std::move(cond)),
          step(std::move(step)), body(std::move(b)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct ReturnStmt : public Stmt {
    std::unique_ptr<Expr> value; // nullptr for void
    ReturnStmt(std::unique_ptr<Expr> val, SourceLocation l) : value(std::move(val)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct BreakStmt : public Stmt {
    BreakStmt(SourceLocation l) { loc = l; }
    void dump(int indent = 0) const override;
};

struct ContinueStmt : public Stmt {
    ContinueStmt(SourceLocation l) { loc = l; }
    void dump(int indent = 0) const override;
};

// Declarations
struct Decl : public ASTNode {};

struct ParamDecl : public ASTNode {
    ParsedType type;
    std::string name;
    ParamDecl(ParsedType t, std::string n, SourceLocation l)
        : type(std::move(t)), name(std::move(n)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct FunctionDecl : public Decl {
    ParsedType returnType;
    std::string name;
    std::vector<ParamDecl> params;
    std::unique_ptr<BlockStmt> body; // nullptr if prototype/extern
    bool isExtern = false;
    bool isMethod = false;
    std::string parentClassName;
    int stackFrameSize = 64;

    FunctionDecl(ParsedType ret, std::string n, std::vector<ParamDecl> p,
                 std::unique_ptr<BlockStmt> b, bool ext, SourceLocation l)
        : returnType(std::move(ret)), name(std::move(n)), params(std::move(p)),
          body(std::move(b)), isExtern(ext) { loc = l; }
    void dump(int indent = 0) const override;
};

struct FieldDecl : public ASTNode {
    ParsedType type;
    std::string name;
    FieldDecl(ParsedType t, std::string n, SourceLocation l)
        : type(std::move(t)), name(std::move(n)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct ClassDecl : public Decl {
    std::string name;
    std::vector<FieldDecl> fields;
    std::vector<std::unique_ptr<FunctionDecl>> methods;
    std::vector<std::unique_ptr<FunctionDecl>> constructors;

    ClassDecl(std::string n, SourceLocation l) : name(std::move(n)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct GlobalVarDecl : public Decl {
    ParsedType type;
    std::string name;
    std::unique_ptr<Expr> initializer;
    GlobalVarDecl(ParsedType t, std::string n, std::unique_ptr<Expr> init, SourceLocation l)
        : type(std::move(t)), name(std::move(n)), initializer(std::move(init)) { loc = l; }
    void dump(int indent = 0) const override;
};

struct Program : public ASTNode {
    std::vector<std::unique_ptr<Decl>> declarations;
    void dump(int indent = 0) const override;
};

} // namespace mcxx

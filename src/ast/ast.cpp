#include "ast.hpp"

namespace mcxx {

static void printIndent(int indent) {
    for (int i = 0; i < indent; ++i) {
        std::cout << "  ";
    }
}

static const char* binOpToString(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Sub: return "-";
        case BinaryOp::Mul: return "*";
        case BinaryOp::Div: return "/";
        case BinaryOp::Mod: return "%";
        case BinaryOp::BitAnd: return "&";
        case BinaryOp::BitOr: return "|";
        case BinaryOp::BitXor: return "^";
        case BinaryOp::Shl: return "<<";
        case BinaryOp::Shr: return ">>";
        case BinaryOp::Eq: return "==";
        case BinaryOp::Ne: return "!=";
        case BinaryOp::Lt: return "<";
        case BinaryOp::Le: return "<=";
        case BinaryOp::Gt: return ">";
        case BinaryOp::Ge: return ">=";
        case BinaryOp::LogAnd: return "&&";
        case BinaryOp::LogOr: return "||";
    }
    return "?";
}

static const char* unaryOpToString(UnaryOp op) {
    switch (op) {
        case UnaryOp::Plus: return "+";
        case UnaryOp::Minus: return "-";
        case UnaryOp::Not: return "!";
        case UnaryOp::BitNot: return "~";
        case UnaryOp::AddressOf: return "&";
        case UnaryOp::Deref: return "*";
        case UnaryOp::PreInc: return "++ (pre)";
        case UnaryOp::PreDec: return "-- (pre)";
        case UnaryOp::PostInc: return "++ (post)";
        case UnaryOp::PostDec: return "-- (post)";
    }
    return "?";
}

static const char* assignOpToString(AssignOp op) {
    switch (op) {
        case AssignOp::Assign: return "=";
        case AssignOp::AddAssign: return "+=";
        case AssignOp::SubAssign: return "-=";
        case AssignOp::MulAssign: return "*=";
        case AssignOp::DivAssign: return "/=";
    }
    return "=";
}

void IntLiteralExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "IntLiteral(" << value << ")\n";
}

void CharLiteralExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "CharLiteral('" << value << "')\n";
}

void StringLiteralExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "StringLiteral(\"" << value << "\")\n";
}

void BoolLiteralExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "BoolLiteral(" << (value ? "true" : "false") << ")\n";
}

void NullptrExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "Nullptr\n";
}

void VariableExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "Variable(" << name << ")\n";
}

void ThisExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "This\n";
}

void UnaryExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "UnaryExpr(" << unaryOpToString(op) << ")\n";
    operand->dump(indent + 1);
}

void BinaryExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "BinaryExpr(" << binOpToString(op) << ")\n";
    left->dump(indent + 1);
    right->dump(indent + 1);
}

void AssignExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "AssignExpr(" << assignOpToString(op) << ")\n";
    target->dump(indent + 1);
    value->dump(indent + 1);
}

void CallExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "CallExpr(" << callee << ")\n";
    for (const auto& arg : args) {
        arg->dump(indent + 1);
    }
}

void MemberExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "MemberExpr(" << (isArrow ? "->" : ".") << memberName << ")\n";
    object->dump(indent + 1);
}

void MethodCallExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "MethodCallExpr(" << (isArrow ? "->" : ".") << methodName << ")\n";
    object->dump(indent + 1);
    for (const auto& arg : args) {
        arg->dump(indent + 1);
    }
}

void IndexExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "IndexExpr\n";
    array->dump(indent + 1);
    index->dump(indent + 1);
}

void NewExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "NewExpr(" << allocatedType.toString() << ")\n";
    for (const auto& arg : constructorArgs) {
        arg->dump(indent + 1);
    }
    if (arraySizeExpr) {
        printIndent(indent + 1);
        std::cout << "[ArraySize]:\n";
        arraySizeExpr->dump(indent + 2);
    }
}

void SizeofExpr::dump(int indent) const {
    printIndent(indent);
    if (isType) {
        std::cout << "SizeofExpr(type: " << targetType.toString() << ")\n";
    } else {
        std::cout << "SizeofExpr(expr):\n";
        targetExpr->dump(indent + 1);
    }
}

void EndlExpr::dump(int indent) const {
    printIndent(indent);
    std::cout << "EndlExpr\n";
}

void CoutStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "CoutStmt\n";
    for (const auto& e : expressions) {
        e->dump(indent + 1);
    }
}

void CinStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "CinStmt\n";
    for (const auto& t : targets) {
        t->dump(indent + 1);
    }
}

void BlockStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "BlockStmt\n";
    for (const auto& s : statements) {
        s->dump(indent + 1);
    }
}

void ExprStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "ExprStmt\n";
    expr->dump(indent + 1);
}

void VarDeclStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "VarDeclStmt(" << type.toString() << " " << name << ")\n";
    if (initializer) {
        initializer->dump(indent + 1);
    }
}

void DeclGroupStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "DeclGroupStmt\n";
    for (const auto& d : decls) {
        d->dump(indent + 1);
    }
}

void IfStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "IfStmt\n";
    condition->dump(indent + 1);
    thenBranch->dump(indent + 1);
    if (elseBranch) {
        printIndent(indent);
        std::cout << "Else\n";
        elseBranch->dump(indent + 1);
    }
}

void WhileStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "WhileStmt\n";
    condition->dump(indent + 1);
    body->dump(indent + 1);
}

void ForStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "ForStmt\n";
    if (init) init->dump(indent + 1);
    if (condition) condition->dump(indent + 1);
    if (step) step->dump(indent + 1);
    body->dump(indent + 1);
}

void ReturnStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "ReturnStmt\n";
    if (value) {
        value->dump(indent + 1);
    }
}

void BreakStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "BreakStmt\n";
}

void ContinueStmt::dump(int indent) const {
    printIndent(indent);
    std::cout << "ContinueStmt\n";
}

void ParamDecl::dump(int indent) const {
    printIndent(indent);
    std::cout << "Param(" << type.toString() << " " << name << ")\n";
}

void FunctionDecl::dump(int indent) const {
    printIndent(indent);
    std::cout << (isExtern ? "Extern " : "") << "FunctionDecl(" << returnType.toString() << " " << name << ")\n";
    for (const auto& p : params) {
        p.dump(indent + 1);
    }
    if (body) {
        body->dump(indent + 1);
    }
}

void FieldDecl::dump(int indent) const {
    printIndent(indent);
    std::cout << "Field(" << type.toString() << " " << name << ")\n";
}

void ClassDecl::dump(int indent) const {
    printIndent(indent);
    std::cout << "ClassDecl(" << name << ")\n";
    for (const auto& f : fields) {
        f.dump(indent + 1);
    }
    for (const auto& c : constructors) {
        c->dump(indent + 1);
    }
    for (const auto& m : methods) {
        m->dump(indent + 1);
    }
}

void GlobalVarDecl::dump(int indent) const {
    printIndent(indent);
    std::cout << "GlobalVarDecl(" << type.toString() << " " << name << ")\n";
    if (initializer) {
        initializer->dump(indent + 1);
    }
}

void Program::dump(int indent) const {
    printIndent(indent);
    std::cout << "Program\n";
    for (const auto& d : declarations) {
        d->dump(indent + 1);
    }
}

} // namespace mcxx

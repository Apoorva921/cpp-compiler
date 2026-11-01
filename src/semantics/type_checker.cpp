#include "type_checker.hpp"
#include <iostream>

namespace mcxx {

TypeChecker::TypeChecker() {
    registerBuiltins();
}

void TypeChecker::registerBuiltins() {
    // printf(char* fmt, ...) -> int
    FunctionSymbol printfSym;
    printfSym.name = "printf";
    printfSym.mangledName = "_printf";
    printfSym.returnType = Type::makeInt();
    printfSym.paramTypes.push_back(Type::makePointer(Type::makeChar()));
    printfSym.paramNames.push_back("fmt");
    printfSym.isExtern = true;
    symTable.declareFunction("printf", printfSym);

    // puts(char* s) -> int
    FunctionSymbol putsSym;
    putsSym.name = "puts";
    putsSym.mangledName = "_puts";
    putsSym.returnType = Type::makeInt();
    putsSym.paramTypes.push_back(Type::makePointer(Type::makeChar()));
    putsSym.paramNames.push_back("s");
    putsSym.isExtern = true;
    symTable.declareFunction("puts", putsSym);

    // putchar(int c) -> int
    FunctionSymbol putcharSym;
    putcharSym.name = "putchar";
    putcharSym.mangledName = "_putchar";
    putcharSym.returnType = Type::makeInt();
    putcharSym.paramTypes.push_back(Type::makeInt());
    putcharSym.paramNames.push_back("c");
    putcharSym.isExtern = true;
    symTable.declareFunction("putchar", putcharSym);

    // scanf(char* fmt, ...) -> int
    FunctionSymbol scanfSym;
    scanfSym.name = "scanf";
    scanfSym.mangledName = "_scanf";
    scanfSym.returnType = Type::makeInt();
    scanfSym.paramTypes.push_back(Type::makePointer(Type::makeChar()));
    scanfSym.paramNames.push_back("fmt");
    scanfSym.isExtern = true;
    symTable.declareFunction("scanf", scanfSym);

    // malloc(int size) -> void*
    FunctionSymbol mallocSym;
    mallocSym.name = "malloc";
    mallocSym.mangledName = "_malloc";
    mallocSym.returnType = Type::makePointer(Type::makeVoid());
    mallocSym.paramTypes.push_back(Type::makeInt());
    mallocSym.paramNames.push_back("size");
    mallocSym.isExtern = true;
    symTable.declareFunction("malloc", mallocSym);

    // free(void* ptr) -> void
    FunctionSymbol freeSym;
    freeSym.name = "free";
    freeSym.mangledName = "_free";
    freeSym.returnType = Type::makeVoid();
    freeSym.paramTypes.push_back(Type::makePointer(Type::makeVoid()));
    freeSym.paramNames.push_back("ptr");
    freeSym.isExtern = true;
    symTable.declareFunction("free", freeSym);

    // exit(int code) -> void
    FunctionSymbol exitSym;
    exitSym.name = "exit";
    exitSym.mangledName = "_exit";
    exitSym.returnType = Type::makeVoid();
    exitSym.paramTypes.push_back(Type::makeInt());
    exitSym.paramNames.push_back("code");
    exitSym.isExtern = true;
    symTable.declareFunction("exit", exitSym);
}

void TypeChecker::error(const std::string& msg, const SourceLocation& loc) {
    std::cerr << loc.filename << ":" << loc.line << ":" << loc.column
              << ": semantic error: " << msg << "\n";
    errorCount++;
}

std::shared_ptr<Type> TypeChecker::resolveType(const ParsedType& pt, const SourceLocation& loc) {
    std::shared_ptr<Type> base;
    switch (pt.baseKind) {
        case BaseTypeKind::Int: base = Type::makeInt(); break;
        case BaseTypeKind::Char: base = Type::makeChar(); break;
        case BaseTypeKind::Bool: base = Type::makeBool(); break;
        case BaseTypeKind::Void: base = Type::makeVoid(); break;
        case BaseTypeKind::UserDefined: {
            auto cls = symTable.lookupClass(pt.name);
            if (!cls) {
                error("Unknown type '" + pt.name + "'", loc);
                return Type::makeInt();
            }
            base = cls;
            break;
        }
    }

    for (int i = 0; i < pt.pointerDepth; ++i) {
        base = Type::makePointer(base);
    }

    if (pt.isArray) {
        base = Type::makeArray(base, pt.arraySize);
    }

    return base;
}

void TypeChecker::firstPassCollectClasses(Program& program) {
    // Register class names
    for (const auto& decl : program.declarations) {
        if (auto cd = dynamic_cast<ClassDecl*>(decl.get())) {
            auto classType = Type::makeClass(cd->name);
            if (!symTable.declareClass(cd->name, classType)) {
                error("Redefinition of class '" + cd->name + "'", cd->loc);
            }
        }
    }

    // Now compute layout for each class
    for (const auto& decl : program.declarations) {
        if (auto cd = dynamic_cast<ClassDecl*>(decl.get())) {
            auto classType = symTable.lookupClass(cd->name);
            int currentOffset = 0;
            for (const auto& field : cd->fields) {
                auto fType = resolveType(field.type, field.loc);
                int sz = fType->size();
                int align = fType->alignment();
                if (align > 0 && (currentOffset % align) != 0) {
                    currentOffset += align - (currentOffset % align);
                }
                FieldInfo fi;
                fi.name = field.name;
                fi.type = fType;
                fi.offset = currentOffset;
                classType->fieldIndex[field.name] = (int)classType->fields.size();
                classType->fields.push_back(fi);
                currentOffset += sz;
            }
            // Pad total struct size to 4-byte boundary
            if (currentOffset % 4 != 0) {
                currentOffset += 4 - (currentOffset % 4);
            }
            if (currentOffset == 0) currentOffset = 4; // minimum 4 bytes
            classType->classSize = currentOffset;
        }
    }
}

void TypeChecker::firstPassCollectFunctions(Program& program) {
    for (const auto& decl : program.declarations) {
        if (auto fn = dynamic_cast<FunctionDecl*>(decl.get())) {
            FunctionSymbol fs;
            fs.name = fn->name;
            fs.mangledName = fn->isExtern ? ("_" + fn->name) : ("_" + fn->name);
            fs.returnType = resolveType(fn->returnType, fn->loc);
            fs.isExtern = fn->isExtern;
            for (const auto& p : fn->params) {
                fs.paramTypes.push_back(resolveType(p.type, p.loc));
                fs.paramNames.push_back(p.name);
            }
            if (!symTable.declareFunction(fn->name, fs)) {
                // If it's a prototype followed by definition, allow it
                auto existing = symTable.lookupFunction(fn->name);
                if (existing && !existing->returnType->equals(*fs.returnType)) {
                    error("Conflicting types for function '" + fn->name + "'", fn->loc);
                }
            }
        } else if (auto cd = dynamic_cast<ClassDecl*>(decl.get())) {
            // Register constructors: ClassName_ClassName
            for (const auto& ctor : cd->constructors) {
                FunctionSymbol fs;
                fs.name = cd->name;
                fs.mangledName = "_" + cd->name + "_" + cd->name;
                fs.returnType = Type::makePointer(symTable.lookupClass(cd->name));
                fs.isMethod = true;
                fs.parentClassName = cd->name;
                // First parameter is implicit 'this'
                fs.paramTypes.push_back(Type::makePointer(symTable.lookupClass(cd->name)));
                fs.paramNames.push_back("this");
                for (const auto& p : ctor->params) {
                    fs.paramTypes.push_back(resolveType(p.type, p.loc));
                    fs.paramNames.push_back(p.name);
                }
                symTable.declareFunction(cd->name + "::" + cd->name, fs);
            }
            // Register methods: ClassName_MethodName
            for (const auto& method : cd->methods) {
                FunctionSymbol fs;
                fs.name = method->name;
                fs.mangledName = "_" + cd->name + "_" + method->name;
                fs.returnType = resolveType(method->returnType, method->loc);
                fs.isMethod = true;
                fs.parentClassName = cd->name;
                // First parameter is implicit 'this'
                fs.paramTypes.push_back(Type::makePointer(symTable.lookupClass(cd->name)));
                fs.paramNames.push_back("this");
                for (const auto& p : method->params) {
                    fs.paramTypes.push_back(resolveType(p.type, p.loc));
                    fs.paramNames.push_back(p.name);
                }
                symTable.declareFunction(cd->name + "::" + method->name, fs);
            }
        } else if (auto gv = dynamic_cast<GlobalVarDecl*>(decl.get())) {
            Symbol sym;
            sym.name = gv->name;
            sym.type = resolveType(gv->type, gv->loc);
            sym.kind = SymbolKind::GlobalVar;
            sym.globalLabel = "_" + gv->name;
            if (!symTable.declareVar(gv->name, sym)) {
                error("Redefinition of global variable '" + gv->name + "'", gv->loc);
            }
        }
    }
}

bool TypeChecker::analyze(Program& program) {
    firstPassCollectClasses(program);
    firstPassCollectFunctions(program);

    // Global variable initializers
    for (const auto& decl : program.declarations) {
        if (auto gv = dynamic_cast<GlobalVarDecl*>(decl.get())) {
            if (gv->initializer) {
                analyzeExpr(*gv->initializer);
            }
        }
    }

    // Function definitions
    for (const auto& decl : program.declarations) {
        if (auto fn = dynamic_cast<FunctionDecl*>(decl.get())) {
            if (fn->body) {
                analyzeFunction(*fn);
            }
        } else if (auto cd = dynamic_cast<ClassDecl*>(decl.get())) {
            currentClassName = cd->name;
            for (const auto& ctor : cd->constructors) {
                if (ctor->body) analyzeFunction(*ctor);
            }
            for (const auto& m : cd->methods) {
                if (m->body) analyzeFunction(*m);
            }
            currentClassName = "";
        }
    }

    return errorCount == 0;
}

void TypeChecker::analyzeFunction(FunctionDecl& fn) {
    currentFunctionReturnType = resolveType(fn.returnType, fn.loc);
    currentStackOffset = 0; // reset local stack offset

    symTable.enterScope();

    // Stack frame convention (cdecl on x86):
    // +8(%ebp)  = first param
    // +12(%ebp) = second param ...
    int paramOffset = 8;

    if (fn.isMethod) {
        // Declare implicit 'this'
        auto classType = symTable.lookupClass(fn.parentClassName);
        Symbol thisSym;
        thisSym.name = "this";
        thisSym.type = Type::makePointer(classType);
        thisSym.kind = SymbolKind::Parameter;
        thisSym.offset = paramOffset;
        symTable.declareVar("this", thisSym);
        paramOffset += 4;
    }

    for (const auto& p : fn.params) {
        Symbol sym;
        sym.name = p.name;
        sym.type = resolveType(p.type, p.loc);
        sym.kind = SymbolKind::Parameter;
        sym.offset = paramOffset;
        symTable.declareVar(p.name, sym);
        paramOffset += 4;
    }

    if (fn.body) {
        analyzeBlock(*fn.body);
    }

    int frame = -currentStackOffset + 64;
    if (frame % 16 != 0) frame += 16 - (frame % 16);
    fn.stackFrameSize = frame;

    symTable.exitScope();
    currentFunctionReturnType = nullptr;
}

void TypeChecker::analyzeBlock(BlockStmt& block) {
    symTable.enterScope();
    for (const auto& stmt : block.statements) {
        analyzeStmt(*stmt);
    }
    symTable.exitScope();
}

void TypeChecker::analyzeStmt(Stmt& stmt) {
    if (auto bs = dynamic_cast<BlockStmt*>(&stmt)) {
        analyzeBlock(*bs);
    } else if (auto es = dynamic_cast<ExprStmt*>(&stmt)) {
        analyzeExpr(*es->expr);
    } else if (auto vds = dynamic_cast<VarDeclStmt*>(&stmt)) {
        auto varType = resolveType(vds->type, vds->loc);
        int sz = varType->size();
        // Align to 4 bytes
        if (sz % 4 != 0) {
            sz += 4 - (sz % 4);
        }
        currentStackOffset -= sz;
        vds->stackOffset = currentStackOffset;

        Symbol sym;
        sym.name = vds->name;
        sym.type = varType;
        sym.kind = SymbolKind::LocalVar;
        sym.offset = currentStackOffset;

        if (!symTable.declareVar(vds->name, sym)) {
            error("Redeclaration of variable '" + vds->name + "'", vds->loc);
        }

        if (vds->initializer) {
            auto initType = analyzeExpr(*vds->initializer);
            // Optional: check type compatibility
        }
    } else if (auto dgs = dynamic_cast<DeclGroupStmt*>(&stmt)) {
        for (const auto& d : dgs->decls) {
            analyzeStmt(*d);
        }
    } else if (auto ifs = dynamic_cast<IfStmt*>(&stmt)) {
        analyzeExpr(*ifs->condition);
        analyzeStmt(*ifs->thenBranch);
        if (ifs->elseBranch) {
            analyzeStmt(*ifs->elseBranch);
        }
    } else if (auto ws = dynamic_cast<WhileStmt*>(&stmt)) {
        analyzeExpr(*ws->condition);
        analyzeStmt(*ws->body);
    } else if (auto fs = dynamic_cast<ForStmt*>(&stmt)) {
        symTable.enterScope();
        if (fs->init) analyzeStmt(*fs->init);
        if (fs->condition) analyzeExpr(*fs->condition);
        if (fs->step) analyzeExpr(*fs->step);
        analyzeStmt(*fs->body);
        symTable.exitScope();
    } else if (auto cs = dynamic_cast<CoutStmt*>(&stmt)) {
        for (const auto& e : cs->expressions) {
            analyzeExpr(*e);
        }
    } else if (auto cs = dynamic_cast<CinStmt*>(&stmt)) {
        for (const auto& t : cs->targets) {
            analyzeExpr(*t);
        }
    } else if (auto rs = dynamic_cast<ReturnStmt*>(&stmt)) {
        if (rs->value) {
            auto valType = analyzeExpr(*rs->value);
            if (currentFunctionReturnType && currentFunctionReturnType->isVoid()) {
                error("Cannot return a value from a void function", rs->loc);
            }
        } else {
            if (currentFunctionReturnType && !currentFunctionReturnType->isVoid()) {
                error("Non-void function should return a value", rs->loc);
            }
        }
    }
}

std::shared_ptr<Type> TypeChecker::analyzeExpr(Expr& expr) {
    if (dynamic_cast<EndlExpr*>(&expr)) {
        expr.resolvedType = Type::makeVoid();
        return expr.resolvedType;
    }
    if (dynamic_cast<IntLiteralExpr*>(&expr)) {
        expr.resolvedType = Type::makeInt();
        return expr.resolvedType;
    }
    if (dynamic_cast<CharLiteralExpr*>(&expr)) {
        expr.resolvedType = Type::makeChar();
        return expr.resolvedType;
    }
    if (dynamic_cast<StringLiteralExpr*>(&expr)) {
        expr.resolvedType = Type::makePointer(Type::makeChar());
        return expr.resolvedType;
    }
    if (dynamic_cast<BoolLiteralExpr*>(&expr)) {
        expr.resolvedType = Type::makeBool();
        return expr.resolvedType;
    }
    if (dynamic_cast<NullptrExpr*>(&expr)) {
        expr.resolvedType = Type::makePointer(Type::makeVoid());
        return expr.resolvedType;
    }
    if (dynamic_cast<ThisExpr*>(&expr)) {
        if (currentClassName.empty()) {
            error("'this' can only be used inside a class method", expr.loc);
            expr.resolvedType = Type::makePointer(Type::makeVoid());
        } else {
            auto cls = symTable.lookupClass(currentClassName);
            expr.resolvedType = Type::makePointer(cls);
        }
        return expr.resolvedType;
    }
    if (auto ve = dynamic_cast<VariableExpr*>(&expr)) {
        auto sym = symTable.lookupVar(ve->name);
        if (sym) {
            expr.resolvedType = sym->type;
            ve->stackOffset = sym->offset;
            ve->globalLabel = sym->globalLabel;
            if (sym->kind == SymbolKind::LocalVar) ve->varKind = VarKind::Local;
            else if (sym->kind == SymbolKind::Parameter) ve->varKind = VarKind::Param;
            else if (sym->kind == SymbolKind::GlobalVar) ve->varKind = VarKind::Global;
            return expr.resolvedType;
        }

        // Check if it's a field of the current class (implicit this->field)
        if (!currentClassName.empty()) {
            auto cls = symTable.lookupClass(currentClassName);
            if (cls && cls->fieldIndex.find(ve->name) != cls->fieldIndex.end()) {
                int idx = cls->fieldIndex[ve->name];
                expr.resolvedType = cls->fields[idx].type;
                ve->varKind = VarKind::Field;
                ve->fieldOffset = cls->fields[idx].offset;
                return expr.resolvedType;
            }
        }

        error("Undeclared identifier '" + ve->name + "'", ve->loc);
        expr.resolvedType = Type::makeInt();
        return expr.resolvedType;
    }
    if (auto ue = dynamic_cast<UnaryExpr*>(&expr)) {
        auto subType = analyzeExpr(*ue->operand);
        if (ue->op == UnaryOp::AddressOf) {
            expr.resolvedType = Type::makePointer(subType);
        } else if (ue->op == UnaryOp::Deref) {
            if (subType->isPointer()) {
                expr.resolvedType = subType->pointeeType;
            } else if (subType->isArray()) {
                expr.resolvedType = subType->pointeeType;
            } else {
                error("Cannot dereference non-pointer type", ue->loc);
                expr.resolvedType = Type::makeInt();
            }
        } else if (ue->op == UnaryOp::Not) {
            expr.resolvedType = Type::makeBool();
        } else {
            expr.resolvedType = subType;
        }
        return expr.resolvedType;
    }
    if (auto be = dynamic_cast<BinaryExpr*>(&expr)) {
        auto leftType = analyzeExpr(*be->left);
        auto rightType = analyzeExpr(*be->right);

        switch (be->op) {
            case BinaryOp::Eq:
            case BinaryOp::Ne:
            case BinaryOp::Lt:
            case BinaryOp::Le:
            case BinaryOp::Gt:
            case BinaryOp::Ge:
            case BinaryOp::LogAnd:
            case BinaryOp::LogOr:
                expr.resolvedType = Type::makeBool();
                break;
            default:
                // Arithmetic / Bitwise
                if (leftType->isPointer()) {
                    expr.resolvedType = leftType; // pointer arithmetic
                } else {
                    expr.resolvedType = leftType;
                }
                break;
        }
        return expr.resolvedType;
    }
    if (auto ae = dynamic_cast<AssignExpr*>(&expr)) {
        auto targetType = analyzeExpr(*ae->target);
        analyzeExpr(*ae->value);
        expr.resolvedType = targetType;
        return expr.resolvedType;
    }
    if (auto ce = dynamic_cast<CallExpr*>(&expr)) {
        for (const auto& a : ce->args) {
            analyzeExpr(*a);
        }

        auto fn = symTable.lookupFunction(ce->callee);
        if (fn) {
            expr.resolvedType = fn->returnType;
            return expr.resolvedType;
        }

        // Check if inside a class method and callee is a member method: this->method(...)
        if (!currentClassName.empty()) {
            auto methodFn = symTable.lookupFunction(currentClassName + "::" + ce->callee);
            if (methodFn) {
                expr.resolvedType = methodFn->returnType;
                return expr.resolvedType;
            }
        }

        error("Call to undefined function '" + ce->callee + "'", ce->loc);
        expr.resolvedType = Type::makeInt();
        return expr.resolvedType;
    }
    if (auto me = dynamic_cast<MemberExpr*>(&expr)) {
        auto objType = analyzeExpr(*me->object);
        std::shared_ptr<Type> clsType = objType;
        if (me->isArrow) {
            if (objType->isPointer()) {
                clsType = objType->pointeeType;
            } else {
                error("Left of '->' must be a pointer to class/struct", me->loc);
            }
        }
        if (clsType && clsType->isClass()) {
            auto it = clsType->fieldIndex.find(me->memberName);
            if (it != clsType->fieldIndex.end()) {
                expr.resolvedType = clsType->fields[it->second].type;
                return expr.resolvedType;
            }
            error("Class '" + clsType->className + "' has no field named '" + me->memberName + "'", me->loc);
        } else {
            error("Member access on non-class type", me->loc);
        }
        expr.resolvedType = Type::makeInt();
        return expr.resolvedType;
    }
    if (auto mce = dynamic_cast<MethodCallExpr*>(&expr)) {
        auto objType = analyzeExpr(*mce->object);
        for (const auto& a : mce->args) {
            analyzeExpr(*a);
        }

        std::shared_ptr<Type> clsType = objType;
        if (mce->isArrow) {
            if (objType->isPointer()) {
                clsType = objType->pointeeType;
            }
        }
        if (clsType && clsType->isClass()) {
            auto method = symTable.lookupFunction(clsType->className + "::" + mce->methodName);
            if (method) {
                expr.resolvedType = method->returnType;
                return expr.resolvedType;
            }
            error("Class '" + clsType->className + "' has no method named '" + mce->methodName + "'", mce->loc);
        } else {
            error("Method call on non-class type", mce->loc);
        }
        expr.resolvedType = Type::makeInt();
        return expr.resolvedType;
    }
    if (auto ie = dynamic_cast<IndexExpr*>(&expr)) {
        auto arrType = analyzeExpr(*ie->array);
        analyzeExpr(*ie->index);
        if (arrType->isArray() || arrType->isPointer()) {
            expr.resolvedType = arrType->pointeeType;
        } else {
            error("Subscripted value is not an array or pointer", ie->loc);
            expr.resolvedType = Type::makeInt();
        }
        return expr.resolvedType;
    }
    if (auto ne = dynamic_cast<NewExpr*>(&expr)) {
        auto baseType = resolveType(ne->allocatedType, ne->loc);
        for (const auto& a : ne->constructorArgs) {
            analyzeExpr(*a);
        }
        if (ne->arraySizeExpr) {
            analyzeExpr(*ne->arraySizeExpr);
            expr.resolvedType = Type::makePointer(baseType);
        } else {
            expr.resolvedType = Type::makePointer(baseType);
        }
        return expr.resolvedType;
    }
    if (dynamic_cast<SizeofExpr*>(&expr)) {
        expr.resolvedType = Type::makeInt();
        return expr.resolvedType;
    }

    expr.resolvedType = Type::makeInt();
    return expr.resolvedType;
}

} // namespace mcxx

#include "codegen.hpp"
#include <iomanip>
#include <iostream>

namespace mcxx {

CodeGenerator::CodeGenerator(SymbolTable& symTable) : symTable(symTable) {}

void CodeGenerator::emitLine(const std::string& line) {
    textOut << "\t" << line << "\n";
}

void CodeGenerator::emitLabel(const std::string& label) {
    textOut << label << ":\n";
}

std::string CodeGenerator::newLabel(const std::string& prefix) {
    return prefix + "_" + std::to_string(++labelCounter);
}

std::string CodeGenerator::addStringLiteral(const std::string& str) {
    std::string label = "LC" + std::to_string(stringLiteralCounter++);
    rdataOut << label << ":\n\t.ascii \"";
    for (char c : str) {
        if (c == '\n') rdataOut << "\\12";
        else if (c == '\t') rdataOut << "\\11";
        else if (c == '\r') rdataOut << "\\15";
        else if (c == '\\') rdataOut << "\\\\";
        else if (c == '"') rdataOut << "\\\"";
        else if (c == '\0') rdataOut << "\\0";
        else rdataOut << c;
    }
    rdataOut << "\\0\"\n";
    return label;
}

std::string CodeGenerator::generate(Program& program) {
    rdataOut << "\t.section .rdata,\"dr\"\n";
    dataOut << "\t.data\n";
    textOut << "\t.text\n";

    // 1. Process Global Variables
    for (const auto& decl : program.declarations) {
        if (auto gv = dynamic_cast<GlobalVarDecl*>(decl.get())) {
            genGlobalVar(*gv);
        }
    }

    // 2. Process Classes (constructors and methods)
    for (const auto& decl : program.declarations) {
        if (auto cd = dynamic_cast<ClassDecl*>(decl.get())) {
            genClass(*cd);
        }
    }

    // 3. Process Functions
    for (const auto& decl : program.declarations) {
        if (auto fn = dynamic_cast<FunctionDecl*>(decl.get())) {
            if (!fn->isExtern && fn->body) {
                genFunction(*fn);
            }
        }
    }

    std::ostringstream fullAsm;
    fullAsm << "\t.file \"mcxx_output.cpp\"\n";
    fullAsm << "\t.def ___main; .scl 2; .type 32; .endef\n";
    fullAsm << rdataOut.str();
    fullAsm << dataOut.str();
    fullAsm << textOut.str();

    return fullAsm.str();
}

void CodeGenerator::genGlobalVar(GlobalVarDecl& gv) {
    std::string label = "_" + gv.name;
    dataOut << "\t.globl\t" << label << "\n";
    dataOut << "\t.align\t4\n";
    dataOut << label << ":\n";

    int sz = 4;
    auto sym = symTable.lookupVar(gv.name);
    if (sym && sym->type) {
        sz = sym->type->size();
    }

    if (gv.initializer) {
        if (auto il = dynamic_cast<IntLiteralExpr*>(gv.initializer.get())) {
            dataOut << "\t.long\t" << il->value << "\n";
        } else if (auto cl = dynamic_cast<CharLiteralExpr*>(gv.initializer.get())) {
            dataOut << "\t.byte\t" << static_cast<int>(cl->value) << "\n";
            int rem = sz - 1;
            if (rem > 0) dataOut << "\t.space\t" << rem << "\n";
        } else if (auto bl = dynamic_cast<BoolLiteralExpr*>(gv.initializer.get())) {
            dataOut << "\t.byte\t" << (bl->value ? 1 : 0) << "\n";
            int rem = sz - 1;
            if (rem > 0) dataOut << "\t.space\t" << rem << "\n";
        } else {
            dataOut << "\t.space\t" << sz << "\n";
        }
    } else {
        dataOut << "\t.space\t" << sz << "\n";
    }
}

void CodeGenerator::genClass(ClassDecl& cd) {
    currentClassName = cd.name;

    for (const auto& ctor : cd.constructors) {
        if (ctor->body) {
            genFunction(*ctor);
        }
    }

    for (const auto& method : cd.methods) {
        if (method->body) {
            genFunction(*method);
        }
    }

    currentClassName = "";
}

void CodeGenerator::genFunction(FunctionDecl& fn) {
    std::string funcLabel;
    if (fn.isMethod) {
        funcLabel = "_" + fn.parentClassName + "_" + fn.name;
    } else {
        funcLabel = "_" + fn.name;
    }

    textOut << "\t.globl\t" << funcLabel << "\n";
    textOut << "\t.def\t" << funcLabel << ";\t.scl\t2;\t.type\t32;\t.endef\n";
    emitLabel(funcLabel);

    // Function Prologue
    emitLine("pushl\t%ebp");
    emitLine("movl\t%esp, %ebp");

    currentFunctionFrameSize = fn.stackFrameSize;
    emitLine("subl\t$" + std::to_string(currentFunctionFrameSize) + ", %esp");

    // MinGW runtime init in main
    if (fn.name == "main" && !fn.isMethod) {
        emitLine("call\t___main");
    }

    if (fn.body) {
        genBlock(*fn.body);
    }

    // Default return
    if (fn.isMethod && fn.name == fn.parentClassName) {
        // Constructor implicitly returns 'this'
        emitLine("movl\t8(%ebp), %eax");
    } else if (fn.name == "main") {
        emitLine("movl\t$0, %eax");
    }

    // Epilogue
    emitLine("leave");
    emitLine("ret");
}

void CodeGenerator::genBlock(BlockStmt& block) {
    for (const auto& stmt : block.statements) {
        genStmt(*stmt);
    }
}

void CodeGenerator::genStmt(Stmt& stmt) {
    if (auto bs = dynamic_cast<BlockStmt*>(&stmt)) {
        genBlock(*bs);
    } else if (auto es = dynamic_cast<ExprStmt*>(&stmt)) {
        genExpr(*es->expr);
    } else if (auto vds = dynamic_cast<VarDeclStmt*>(&stmt)) {
        if (vds->initializer) {
            genExpr(*vds->initializer);
            emitLine("movl\t%eax, " + std::to_string(vds->stackOffset) + "(%ebp)");
        } else {
            emitLine("movl\t$0, " + std::to_string(vds->stackOffset) + "(%ebp)");
        }
    } else if (auto dgs = dynamic_cast<DeclGroupStmt*>(&stmt)) {
        for (const auto& d : dgs->decls) {
            genStmt(*d);
        }
    } else if (auto ifs = dynamic_cast<IfStmt*>(&stmt)) {
        std::string elseLabel = newLabel("L_else");
        std::string endLabel = newLabel("L_endif");

        genExpr(*ifs->condition);
        emitLine("cmpl\t$0, %eax");
        emitLine("je\t" + (ifs->elseBranch ? elseLabel : endLabel));

        genStmt(*ifs->thenBranch);

        if (ifs->elseBranch) {
            emitLine("jmp\t" + endLabel);
            emitLabel(elseLabel);
            genStmt(*ifs->elseBranch);
        }
        emitLabel(endLabel);
    } else if (auto ws = dynamic_cast<WhileStmt*>(&stmt)) {
        std::string condLabel = newLabel("L_while_cond");
        std::string endLabel = newLabel("L_while_end");

        std::string savedBreak = currentBreakLabel;
        std::string savedContinue = currentContinueLabel;
        currentBreakLabel = endLabel;
        currentContinueLabel = condLabel;

        emitLabel(condLabel);
        genExpr(*ws->condition);
        emitLine("cmpl\t$0, %eax");
        emitLine("je\t" + endLabel);

        genStmt(*ws->body);
        emitLine("jmp\t" + condLabel);

        emitLabel(endLabel);

        currentBreakLabel = savedBreak;
        currentContinueLabel = savedContinue;
    } else if (auto fs = dynamic_cast<ForStmt*>(&stmt)) {
        std::string condLabel = newLabel("L_for_cond");
        std::string stepLabel = newLabel("L_for_step");
        std::string endLabel = newLabel("L_for_end");

        std::string savedBreak = currentBreakLabel;
        std::string savedContinue = currentContinueLabel;
        currentBreakLabel = endLabel;
        currentContinueLabel = stepLabel;

        if (fs->init) {
            genStmt(*fs->init);
        }

        emitLabel(condLabel);
        if (fs->condition) {
            genExpr(*fs->condition);
            emitLine("cmpl\t$0, %eax");
            emitLine("je\t" + endLabel);
        }

        genStmt(*fs->body);

        emitLabel(stepLabel);
        if (fs->step) {
            genExpr(*fs->step);
        }
        emitLine("jmp\t" + condLabel);

        emitLabel(endLabel);

        currentBreakLabel = savedBreak;
        currentContinueLabel = savedContinue;
    } else if (auto cs = dynamic_cast<CoutStmt*>(&stmt)) {
        for (const auto& e : cs->expressions) {
            if (dynamic_cast<EndlExpr*>(e.get())) {
                emitLine("pushl\t$10"); // '\n'
                emitLine("call\t_putchar");
                emitLine("addl\t$4, %esp");
            } else if (e->resolvedType && e->resolvedType->isPointer() &&
                       e->resolvedType->pointeeType && e->resolvedType->pointeeType->isChar()) {
                genExpr(*e);
                emitLine("pushl\t%eax");
                std::string fmt = addStringLiteral("%s");
                emitLine("pushl\t$" + fmt);
                emitLine("call\t_printf");
                emitLine("addl\t$8, %esp");
            } else if (e->resolvedType && e->resolvedType->isChar()) {
                genExpr(*e);
                emitLine("pushl\t%eax");
                emitLine("call\t_putchar");
                emitLine("addl\t$4, %esp");
            } else {
                genExpr(*e);
                emitLine("pushl\t%eax");
                std::string fmt = addStringLiteral("%d");
                emitLine("pushl\t$" + fmt);
                emitLine("call\t_printf");
                emitLine("addl\t$8, %esp");
            }
        }
    } else if (auto cs = dynamic_cast<CinStmt*>(&stmt)) {
        for (const auto& t : cs->targets) {
            genLValue(*t);
            emitLine("pushl\t%edx");
            std::string fmt = addStringLiteral("%d");
            emitLine("pushl\t$" + fmt);
            emitLine("call\t_scanf");
            emitLine("addl\t$8, %esp");
        }
    } else if (auto rs = dynamic_cast<ReturnStmt*>(&stmt)) {
        if (rs->value) {
            genExpr(*rs->value);
        }
        emitLine("leave");
        emitLine("ret");
    } else if (dynamic_cast<BreakStmt*>(&stmt)) {
        if (!currentBreakLabel.empty()) {
            emitLine("jmp\t" + currentBreakLabel);
        }
    } else if (dynamic_cast<ContinueStmt*>(&stmt)) {
        if (!currentContinueLabel.empty()) {
            emitLine("jmp\t" + currentContinueLabel);
        }
    }
}

// Compute memory address of lvalue and leave in %edx
void CodeGenerator::genLValue(Expr& expr) {
    if (auto ve = dynamic_cast<VariableExpr*>(&expr)) {
        if (ve->varKind == VarKind::Local || ve->varKind == VarKind::Param) {
            emitLine("leal\t" + std::to_string(ve->stackOffset) + "(%ebp), %edx");
            return;
        } else if (ve->varKind == VarKind::Global) {
            emitLine("movl\t$" + ve->globalLabel + ", %edx");
            return;
        } else if (ve->varKind == VarKind::Field) {
            emitLine("movl\t8(%ebp), %edx"); // load 'this'
            if (ve->fieldOffset > 0) {
                emitLine("addl\t$" + std::to_string(ve->fieldOffset) + ", %edx");
            }
            return;
        }
        emitLine("movl\t$0, %edx");
    } else if (auto ue = dynamic_cast<UnaryExpr*>(&expr)) {
        if (ue->op == UnaryOp::Deref) {
            // *ptr -> evaluate ptr into %eax, then move to %edx
            genExpr(*ue->operand);
            emitLine("movl\t%eax, %edx");
            return;
        }
    } else if (auto me = dynamic_cast<MemberExpr*>(&expr)) {
        auto objType = me->object->resolvedType;
        std::shared_ptr<Type> clsType = objType;
        if (me->isArrow && objType && objType->isPointer()) {
            clsType = objType->pointeeType;
        }
        int fieldOffset = 0;
        if (clsType && clsType->fieldIndex.find(me->memberName) != clsType->fieldIndex.end()) {
            fieldOffset = clsType->fields[clsType->fieldIndex[me->memberName]].offset;
        }

        if (me->isArrow) {
            // ptr->field: evaluate ptr into %eax
            genExpr(*me->object);
            emitLine("movl\t%eax, %edx");
        } else {
            // obj.field: compute address of obj into %edx
            genLValue(*me->object);
        }
        if (fieldOffset > 0) {
            emitLine("addl\t$" + std::to_string(fieldOffset) + ", %edx");
        }
        return;
    } else if (auto ie = dynamic_cast<IndexExpr*>(&expr)) {
        // array[index]
        auto arrType = ie->array->resolvedType;
        int elemSize = 4;
        if (arrType && arrType->pointeeType) {
            elemSize = arrType->pointeeType->size();
        }

        if (arrType && arrType->isArray()) {
            genLValue(*ie->array); // address of array on stack/data
        } else {
            genExpr(*ie->array); // pointer value in %eax
            emitLine("movl\t%eax, %edx");
        }

        emitLine("pushl\t%edx"); // save base address
        genExpr(*ie->index);    // index in %eax
        emitLine("popl\t%edx");  // restore base address

        if (elemSize == 1) {
            emitLine("leal\t(%edx, %eax, 1), %edx");
        } else if (elemSize == 2) {
            emitLine("leal\t(%edx, %eax, 2), %edx");
        } else {
            emitLine("leal\t(%edx, %eax, 4), %edx");
        }
        return;
    }
}

void CodeGenerator::genExpr(Expr& expr) {
    if (auto il = dynamic_cast<IntLiteralExpr*>(&expr)) {
        emitLine("movl\t$" + std::to_string(il->value) + ", %eax");
    } else if (auto cl = dynamic_cast<CharLiteralExpr*>(&expr)) {
        emitLine("movl\t$" + std::to_string(static_cast<int>(cl->value)) + ", %eax");
    } else if (auto sl = dynamic_cast<StringLiteralExpr*>(&expr)) {
        std::string label = addStringLiteral(sl->value);
        emitLine("movl\t$" + label + ", %eax");
    } else if (auto bl = dynamic_cast<BoolLiteralExpr*>(&expr)) {
        emitLine("movl\t$" + std::string(bl->value ? "1" : "0") + ", %eax");
    } else if (dynamic_cast<NullptrExpr*>(&expr)) {
        emitLine("movl\t$0, %eax");
    } else if (dynamic_cast<ThisExpr*>(&expr)) {
        emitLine("movl\t8(%ebp), %eax");
    } else if (auto ve = dynamic_cast<VariableExpr*>(&expr)) {
        if (ve->varKind == VarKind::Local || ve->varKind == VarKind::Param) {
            if (ve->resolvedType && ve->resolvedType->isArray()) {
                emitLine("leal\t" + std::to_string(ve->stackOffset) + "(%ebp), %eax");
            } else if (ve->resolvedType && (ve->resolvedType->isChar() || ve->resolvedType->isBool())) {
                emitLine("movsbl\t" + std::to_string(ve->stackOffset) + "(%ebp), %eax");
            } else {
                emitLine("movl\t" + std::to_string(ve->stackOffset) + "(%ebp), %eax");
            }
            return;
        } else if (ve->varKind == VarKind::Global) {
            if (ve->resolvedType && ve->resolvedType->isArray()) {
                emitLine("movl\t$" + ve->globalLabel + ", %eax");
            } else {
                emitLine("movl\t" + ve->globalLabel + ", %eax");
            }
            return;
        } else if (ve->varKind == VarKind::Field) {
            emitLine("movl\t8(%ebp), %edx"); // 'this'
            emitLine("movl\t" + std::to_string(ve->fieldOffset) + "(%edx), %eax");
            return;
        }
    } else if (auto ue = dynamic_cast<UnaryExpr*>(&expr)) {
        switch (ue->op) {
            case UnaryOp::Plus:
                genExpr(*ue->operand);
                break;
            case UnaryOp::Minus:
                genExpr(*ue->operand);
                emitLine("negl\t%eax");
                break;
            case UnaryOp::Not:
                genExpr(*ue->operand);
                emitLine("cmpl\t$0, %eax");
                emitLine("sete\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            case UnaryOp::BitNot:
                genExpr(*ue->operand);
                emitLine("notl\t%eax");
                break;
            case UnaryOp::AddressOf:
                genLValue(*ue->operand);
                emitLine("movl\t%edx, %eax");
                break;
            case UnaryOp::Deref:
                genExpr(*ue->operand); // pointer in %eax
                if (ue->resolvedType && (ue->resolvedType->isChar() || ue->resolvedType->isBool())) {
                    emitLine("movsbl\t(%eax), %eax");
                } else {
                    emitLine("movl\t(%eax), %eax");
                }
                break;
            case UnaryOp::PreInc:
                genLValue(*ue->operand);
                emitLine("movl\t(%edx), %eax");
                emitLine("addl\t$1, %eax");
                emitLine("movl\t%eax, (%edx)");
                break;
            case UnaryOp::PreDec:
                genLValue(*ue->operand);
                emitLine("movl\t(%edx), %eax");
                emitLine("subl\t$1, %eax");
                emitLine("movl\t%eax, (%edx)");
                break;
            case UnaryOp::PostInc:
                genLValue(*ue->operand);
                emitLine("movl\t(%edx), %eax"); // return old value
                emitLine("movl\t%eax, %ecx");
                emitLine("addl\t$1, %ecx");
                emitLine("movl\t%ecx, (%edx)");
                break;
            case UnaryOp::PostDec:
                genLValue(*ue->operand);
                emitLine("movl\t(%edx), %eax"); // return old value
                emitLine("movl\t%eax, %ecx");
                emitLine("subl\t$1, %ecx");
                emitLine("movl\t%ecx, (%edx)");
                break;
        }
    } else if (auto be = dynamic_cast<BinaryExpr*>(&expr)) {
        // Short-circuit logical operators
        if (be->op == BinaryOp::LogAnd) {
            std::string falseLabel = newLabel("L_logand_false");
            std::string endLabel = newLabel("L_logand_end");
            genExpr(*be->left);
            emitLine("cmpl\t$0, %eax");
            emitLine("je\t" + falseLabel);
            genExpr(*be->right);
            emitLine("cmpl\t$0, %eax");
            emitLine("je\t" + falseLabel);
            emitLine("movl\t$1, %eax");
            emitLine("jmp\t" + endLabel);
            emitLabel(falseLabel);
            emitLine("movl\t$0, %eax");
            emitLabel(endLabel);
            return;
        }
        if (be->op == BinaryOp::LogOr) {
            std::string trueLabel = newLabel("L_logor_true");
            std::string endLabel = newLabel("L_logor_end");
            genExpr(*be->left);
            emitLine("cmpl\t$0, %eax");
            emitLine("jne\t" + trueLabel);
            genExpr(*be->right);
            emitLine("cmpl\t$0, %eax");
            emitLine("jne\t" + trueLabel);
            emitLine("movl\t$0, %eax");
            emitLine("jmp\t" + endLabel);
            emitLabel(trueLabel);
            emitLine("movl\t$1, %eax");
            emitLabel(endLabel);
            return;
        }

        // Pointer arithmetic scaling: ptr + int or ptr - int
        int rightScale = 1;
        if (be->left->resolvedType && be->left->resolvedType->isPointer() &&
            be->left->resolvedType->pointeeType) {
            rightScale = be->left->resolvedType->pointeeType->size();
        }

        genExpr(*be->left);
        emitLine("pushl\t%eax");

        genExpr(*be->right);
        if (rightScale > 1 && (be->op == BinaryOp::Add || be->op == BinaryOp::Sub)) {
            emitLine("imull\t$" + std::to_string(rightScale) + ", %eax");
        }

        emitLine("movl\t%eax, %ecx");
        emitLine("popl\t%eax");

        switch (be->op) {
            case BinaryOp::Add:
                emitLine("addl\t%ecx, %eax");
                break;
            case BinaryOp::Sub:
                emitLine("subl\t%ecx, %eax");
                break;
            case BinaryOp::Mul:
                emitLine("imull\t%ecx, %eax");
                break;
            case BinaryOp::Div:
                emitLine("cdq");
                emitLine("idivl\t%ecx");
                break;
            case BinaryOp::Mod:
                emitLine("cdq");
                emitLine("idivl\t%ecx");
                emitLine("movl\t%edx, %eax");
                break;
            case BinaryOp::BitAnd:
                emitLine("andl\t%ecx, %eax");
                break;
            case BinaryOp::BitOr:
                emitLine("orl\t%ecx, %eax");
                break;
            case BinaryOp::BitXor:
                emitLine("xorl\t%ecx, %eax");
                break;
            case BinaryOp::Shl:
                emitLine("sall\t%cl, %eax");
                break;
            case BinaryOp::Shr:
                emitLine("sarl\t%cl, %eax");
                break;
            case BinaryOp::Eq:
                emitLine("cmpl\t%ecx, %eax");
                emitLine("sete\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            case BinaryOp::Ne:
                emitLine("cmpl\t%ecx, %eax");
                emitLine("setne\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            case BinaryOp::Lt:
                emitLine("cmpl\t%ecx, %eax");
                emitLine("setl\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            case BinaryOp::Le:
                emitLine("cmpl\t%ecx, %eax");
                emitLine("setle\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            case BinaryOp::Gt:
                emitLine("cmpl\t%ecx, %eax");
                emitLine("setg\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            case BinaryOp::Ge:
                emitLine("cmpl\t%ecx, %eax");
                emitLine("setge\t%al");
                emitLine("movzbl\t%al, %eax");
                break;
            default:
                break;
        }
    } else if (auto ae = dynamic_cast<AssignExpr*>(&expr)) {
        genLValue(*ae->target);
        emitLine("pushl\t%edx"); // save address of target

        genExpr(*ae->value);

        if (ae->op == AssignOp::Assign) {
            emitLine("popl\t%edx");
            emitLine("movl\t%eax, (%edx)");
        } else {
            emitLine("movl\t%eax, %ecx");
            emitLine("popl\t%edx");
            emitLine("movl\t(%edx), %eax");
            switch (ae->op) {
                case AssignOp::AddAssign: emitLine("addl\t%ecx, %eax"); break;
                case AssignOp::SubAssign: emitLine("subl\t%ecx, %eax"); break;
                case AssignOp::MulAssign: emitLine("imull\t%ecx, %eax"); break;
                case AssignOp::DivAssign:
                    emitLine("cdq");
                    emitLine("idivl\t%ecx");
                    break;
                default: break;
            }
            emitLine("movl\t%eax, (%edx)");
        }
    } else if (auto ce = dynamic_cast<CallExpr*>(&expr)) {
        // Evaluate args right-to-left and push onto stack
        for (int i = (int)ce->args.size() - 1; i >= 0; --i) {
            genExpr(*ce->args[i]);
            emitLine("pushl\t%eax");
        }

        std::string callTarget = "_" + ce->callee;
        // Check if member method in current class
        if (!currentClassName.empty()) {
            auto methodFn = symTable.lookupFunction(currentClassName + "::" + ce->callee);
            if (methodFn) {
                // push 'this' pointer as first argument!
                emitLine("movl\t8(%ebp), %eax");
                emitLine("pushl\t%eax");
                callTarget = "_" + currentClassName + "_" + ce->callee;
                emitLine("call\t" + callTarget);
                emitLine("addl\t$" + std::to_string((ce->args.size() + 1) * 4) + ", %esp");
                return;
            }
        }

        emitLine("call\t" + callTarget);
        if (!ce->args.empty()) {
            emitLine("addl\t$" + std::to_string(ce->args.size() * 4) + ", %esp");
        }
    } else if (auto me = dynamic_cast<MemberExpr*>(&expr)) {
        genLValue(*me);
        if (me->resolvedType && (me->resolvedType->isChar() || me->resolvedType->isBool())) {
            emitLine("movsbl\t(%edx), %eax");
        } else {
            emitLine("movl\t(%edx), %eax");
        }
    } else if (auto mce = dynamic_cast<MethodCallExpr*>(&expr)) {
        // Push arguments right-to-left
        for (int i = (int)mce->args.size() - 1; i >= 0; --i) {
            genExpr(*mce->args[i]);
            emitLine("pushl\t%eax");
        }

        // Push 'this' pointer
        if (mce->isArrow) {
            genExpr(*mce->object); // pointer value in %eax
        } else {
            genLValue(*mce->object); // address of object in %edx
            emitLine("movl\t%edx, %eax");
        }
        emitLine("pushl\t%eax");

        // Determine class name
        auto objType = mce->object->resolvedType;
        std::string cName;
        if (mce->isArrow && objType && objType->isPointer()) {
            cName = objType->pointeeType->className;
        } else if (objType) {
            cName = objType->className;
        }

        std::string target = "_" + cName + "_" + mce->methodName;
        emitLine("call\t" + target);
        emitLine("addl\t$" + std::to_string((mce->args.size() + 1) * 4) + ", %esp");
    } else if (auto ie = dynamic_cast<IndexExpr*>(&expr)) {
        genLValue(*ie);
        if (ie->resolvedType && (ie->resolvedType->isChar() || ie->resolvedType->isBool())) {
            emitLine("movsbl\t(%edx), %eax");
        } else {
            emitLine("movl\t(%edx), %eax");
        }
    } else if (auto ne = dynamic_cast<NewExpr*>(&expr)) {
        if (ne->arraySizeExpr) {
            // new T[size]
            int elemSize = 4;
            auto baseType = symTable.lookupClass(ne->allocatedType.name);
            if (baseType) elemSize = baseType->size();
            else if (ne->allocatedType.baseKind == BaseTypeKind::Char) elemSize = 1;

            genExpr(*ne->arraySizeExpr);
            if (elemSize > 1) {
                emitLine("imull\t$" + std::to_string(elemSize) + ", %eax");
            }
            emitLine("pushl\t%eax");
            emitLine("call\t_malloc");
            emitLine("addl\t$4, %esp");
        } else {
            // new ClassName(args)
            auto cls = symTable.lookupClass(ne->allocatedType.name);
            int sz = cls ? cls->size() : 4;
            emitLine("pushl\t$" + std::to_string(sz));
            emitLine("call\t_malloc");
            emitLine("addl\t$4, %esp");

            // Save allocated pointer
            emitLine("pushl\t%eax");

            // Check if constructor exists
            std::string ctorName = ne->allocatedType.name + "::" + ne->allocatedType.name;
            auto ctor = symTable.lookupFunction(ctorName);
            if (ctor) {
                // Push args right-to-left
                for (int i = (int)ne->constructorArgs.size() - 1; i >= 0; --i) {
                    genExpr(*ne->constructorArgs[i]);
                    emitLine("pushl\t%eax");
                }
                // Push 'this' (the allocated pointer, currently at top of stack offset)
                int stackOffsetFromTop = (int)ne->constructorArgs.size() * 4;
                emitLine("movl\t" + std::to_string(stackOffsetFromTop) + "(%esp), %eax");
                emitLine("pushl\t%eax");

                std::string target = "_" + ne->allocatedType.name + "_" + ne->allocatedType.name;
                emitLine("call\t" + target);
                emitLine("addl\t$" + std::to_string((ne->constructorArgs.size() + 1) * 4) + ", %esp");
            }

            // Restore allocated pointer
            emitLine("popl\t%eax");
        }
    } else if (auto se = dynamic_cast<SizeofExpr*>(&expr)) {
        int sz = 4;
        if (se->isType) {
            switch (se->targetType.baseKind) {
                case BaseTypeKind::Char:
                case BaseTypeKind::Bool: sz = 1; break;
                case BaseTypeKind::Int: sz = 4; break;
                case BaseTypeKind::UserDefined: {
                    auto c = symTable.lookupClass(se->targetType.name);
                    if (c) sz = c->size();
                    break;
                }
                default: sz = 4; break;
            }
            if (se->targetType.pointerDepth > 0) sz = 4;
        } else if (se->targetExpr && se->targetExpr->resolvedType) {
            sz = se->targetExpr->resolvedType->size();
        }
        emitLine("movl\t$" + std::to_string(sz) + ", %eax");
    }
}

} // namespace mcxx

#include "parser.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace mcxx {

Parser::Parser(std::vector<Token> tokens) : tokens(std::move(tokens)), cursor(0) {}

const Token& Parser::peek(int offset) const {
    if (cursor + offset >= tokens.size()) {
        return tokens.back(); // EOF token
    }
    return tokens[cursor + offset];
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        cursor++;
    }
    return tokens[cursor - 1];
}

bool Parser::match(TokenKind kind) {
    if (check(kind)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::check(TokenKind kind) const {
    if (isAtEnd()) return false;
    return peek().kind == kind;
}

const Token& Parser::expect(TokenKind kind, const std::string& errMsg) {
    if (check(kind)) {
        return advance();
    }
    error(errMsg + " (got '" + peek().text + "')", peek().loc);
    throw std::runtime_error("Parser error");
}

bool Parser::isAtEnd() const {
    return cursor >= tokens.size() || peek().kind == TokenKind::Eof;
}

void Parser::error(const std::string& msg, const SourceLocation& loc) {
    std::cerr << loc.filename << ":" << loc.line << ":" << loc.column
              << ": error: " << msg << "\n";
}

bool Parser::isTypeSpecifier(TokenKind kind) const {
    switch (kind) {
        case TokenKind::KwInt:
        case TokenKind::KwChar:
        case TokenKind::KwBool:
        case TokenKind::KwVoid:
            return true;
        case TokenKind::Identifier:
            // An identifier could be a user-defined class/struct type
            return true;
        default:
            return false;
    }
}

ParsedType Parser::parseType() {
    // Skip optional namespace: std::
    if (check(TokenKind::Identifier) && peek(1).kind == TokenKind::ColonColon) {
        advance(); // skip namespace
        advance(); // skip ::
    }

    ParsedType type;
    const Token& tok = advance();

    if (tok.kind == TokenKind::KwInt) {
        type.baseKind = BaseTypeKind::Int;
    } else if (tok.kind == TokenKind::KwChar) {
        type.baseKind = BaseTypeKind::Char;
    } else if (tok.kind == TokenKind::KwBool) {
        type.baseKind = BaseTypeKind::Bool;
    } else if (tok.kind == TokenKind::KwVoid) {
        type.baseKind = BaseTypeKind::Void;
    } else if (tok.kind == TokenKind::Identifier) {
        type.baseKind = BaseTypeKind::UserDefined;
        type.name = tok.text;
    } else {
        error("Expected type specifier", tok.loc);
        throw std::runtime_error("Invalid type");
    }

    // Pointer stars: int* or char**
    while (match(TokenKind::Star)) {
        type.pointerDepth++;
    }

    return type;
}

std::unique_ptr<Program> Parser::parseProgram() {
    auto prog = std::make_unique<Program>();
    prog->loc = peek().loc;

    while (!isAtEnd()) {
        try {
            auto decl = parseTopLevelDecl();
            if (decl) {
                prog->declarations.push_back(std::move(decl));
            }
        } catch (const std::exception& e) {
            // Panic mode recovery: skip to next semicolon or closing brace
            while (!isAtEnd() && peek().kind != TokenKind::Semicolon && peek().kind != TokenKind::RBrace) {
                advance();
            }
            if (match(TokenKind::Semicolon) || match(TokenKind::RBrace)) {
                // consumed
            }
        }
    }

    return prog;
}

std::unique_ptr<Decl> Parser::parseTopLevelDecl() {
    if (match(TokenKind::Semicolon)) {
        return nullptr;
    }

    // using namespace std;
    if (check(TokenKind::Identifier) && peek().text == "using") {
        while (!isAtEnd() && !match(TokenKind::Semicolon)) {
            advance();
        }
        return nullptr;
    }

    if (check(TokenKind::KwExtern)) {
        return parseExternDecl();
    }

    if (check(TokenKind::KwClass) || check(TokenKind::KwStruct)) {
        return parseClassOrStructDecl();
    }

    return parseFunctionOrGlobalVar();
}

std::unique_ptr<FunctionDecl> Parser::parseExternDecl() {
    SourceLocation loc = advance().loc; // consume 'extern'
    // Optional string literal like "C"
    if (check(TokenKind::StringLiteral)) {
        advance();
    }

    ParsedType retType = parseType();
    const Token& nameTok = expect(TokenKind::Identifier, "Expected function name in extern declaration");
    expect(TokenKind::LParen, "Expected '(' after function name");

    std::vector<ParamDecl> params;
    while (!check(TokenKind::RParen) && !isAtEnd()) {
        if (match(TokenKind::Dot) && match(TokenKind::Dot) && match(TokenKind::Dot)) {
            // varargs ...
            break;
        }
        ParsedType pType = parseType();
        std::string pName;
        if (check(TokenKind::Identifier)) {
            pName = advance().text;
        }
        params.emplace_back(pType, pName, pType.pointerDepth > 0 ? nameTok.loc : nameTok.loc);
        if (!match(TokenKind::Comma)) {
            break;
        }
    }
    expect(TokenKind::RParen, "Expected ')' after parameters");
    expect(TokenKind::Semicolon, "Expected ';' after extern function declaration");

    return std::make_unique<FunctionDecl>(retType, nameTok.text, std::move(params), nullptr, true, loc);
}

std::unique_ptr<ClassDecl> Parser::parseClassOrStructDecl() {
    SourceLocation loc = advance().loc; // consume 'class' or 'struct'
    const Token& nameTok = expect(TokenKind::Identifier, "Expected class/struct name");
    auto classDecl = std::make_unique<ClassDecl>(nameTok.text, loc);

    expect(TokenKind::LBrace, "Expected '{' to start class/struct body");

    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        // Access specifiers
        if (check(TokenKind::KwPublic) || check(TokenKind::KwPrivate)) {
            advance();
            expect(TokenKind::Colon, "Expected ':' after access specifier");
            continue;
        }

        if (match(TokenKind::Semicolon)) continue;

        // Constructor check: ClassName(...)
        if (check(TokenKind::Identifier) && peek().text == classDecl->name && peek(1).kind == TokenKind::LParen) {
            SourceLocation ctorLoc = advance().loc; // class name
            advance(); // '('
            std::vector<ParamDecl> params;
            while (!check(TokenKind::RParen) && !isAtEnd()) {
                ParsedType pType = parseType();
                const Token& pName = expect(TokenKind::Identifier, "Expected parameter name");
                params.emplace_back(pType, pName.text, pName.loc);
                if (!match(TokenKind::Comma)) break;
            }
            expect(TokenKind::RParen, "Expected ')' after constructor parameters");
            auto body = parseBlockStatement();
            ParsedType voidType;
            voidType.baseKind = BaseTypeKind::Void;
            auto ctor = std::make_unique<FunctionDecl>(voidType, classDecl->name, std::move(params), std::move(body), false, ctorLoc);
            ctor->isMethod = true;
            ctor->parentClassName = classDecl->name;
            classDecl->constructors.push_back(std::move(ctor));
            continue;
        }

        // Method or Field
        ParsedType memberType = parseType();
        const Token& memberName = expect(TokenKind::Identifier, "Expected member name");

        if (check(TokenKind::LParen)) {
            // Member method!
            advance(); // '('
            std::vector<ParamDecl> params;
            while (!check(TokenKind::RParen) && !isAtEnd()) {
                ParsedType pType = parseType();
                const Token& pName = expect(TokenKind::Identifier, "Expected parameter name");
                params.emplace_back(pType, pName.text, pName.loc);
                if (!match(TokenKind::Comma)) break;
            }
            expect(TokenKind::RParen, "Expected ')' after method parameters");
            auto body = parseBlockStatement();
            auto method = std::make_unique<FunctionDecl>(memberType, memberName.text, std::move(params), std::move(body), false, memberName.loc);
            method->isMethod = true;
            method->parentClassName = classDecl->name;
            classDecl->methods.push_back(std::move(method));
        } else {
            // Member field
            // Check for array field: int arr[10];
            if (match(TokenKind::LBracket)) {
                memberType.isArray = true;
                const Token& sizeTok = expect(TokenKind::IntLiteral, "Expected integer array size");
                memberType.arraySize = sizeTok.intValue;
                expect(TokenKind::RBracket, "Expected ']' after array size");
            }
            expect(TokenKind::Semicolon, "Expected ';' after member variable declaration");
            classDecl->fields.emplace_back(memberType, memberName.text, memberName.loc);
        }
    }

    expect(TokenKind::RBrace, "Expected '}' at end of class/struct declaration");
    match(TokenKind::Semicolon); // optional trailing semicolon for class definition
    return classDecl;
}

std::unique_ptr<Decl> Parser::parseFunctionOrGlobalVar() {
    SourceLocation loc = peek().loc;
    ParsedType type = parseType();
    const Token& nameTok = expect(TokenKind::Identifier, "Expected identifier after type");

    // Check if function: name(params)
    if (check(TokenKind::LParen)) {
        advance(); // '('
        std::vector<ParamDecl> params;
        while (!check(TokenKind::RParen) && !isAtEnd()) {
            ParsedType pType = parseType();
            const Token& pName = expect(TokenKind::Identifier, "Expected parameter name");
            params.emplace_back(pType, pName.text, pName.loc);
            if (!match(TokenKind::Comma)) break;
        }
        expect(TokenKind::RParen, "Expected ')' after parameters");

        if (match(TokenKind::Semicolon)) {
            // Prototype / forward declaration
            return std::make_unique<FunctionDecl>(type, nameTok.text, std::move(params), nullptr, false, loc);
        }

        auto body = parseBlockStatement();
        return std::make_unique<FunctionDecl>(type, nameTok.text, std::move(params), std::move(body), false, loc);
    }

    // Global variable
    if (match(TokenKind::LBracket)) {
        type.isArray = true;
        const Token& sizeTok = expect(TokenKind::IntLiteral, "Expected integer array size");
        type.arraySize = sizeTok.intValue;
        expect(TokenKind::RBracket, "Expected ']' after array size");
    }

    std::unique_ptr<Expr> init = nullptr;
    if (match(TokenKind::Equal)) {
        init = parseExpression();
    }
    expect(TokenKind::Semicolon, "Expected ';' after global variable declaration");
    return std::make_unique<GlobalVarDecl>(type, nameTok.text, std::move(init), loc);
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (check(TokenKind::LBrace)) {
        return parseBlockStatement();
    }
    if (check(TokenKind::KwIf)) {
        return parseIfStatement();
    }
    if (check(TokenKind::KwWhile)) {
        return parseWhileStatement();
    }
    if (check(TokenKind::KwFor)) {
        return parseForStatement();
    }
    if (check(TokenKind::KwReturn)) {
        return parseReturnStatement();
    }
    if (check(TokenKind::KwBreak)) {
        SourceLocation loc = advance().loc;
        expect(TokenKind::Semicolon, "Expected ';' after break");
        return std::make_unique<BreakStmt>(loc);
    }
    if (check(TokenKind::KwContinue)) {
        SourceLocation loc = advance().loc;
        expect(TokenKind::Semicolon, "Expected ';' after continue");
        return std::make_unique<ContinueStmt>(loc);
    }

    // cout << ... or std::cout << ...
    int coutOffset = 0;
    if (check(TokenKind::Identifier) && peek().text == "std" && peek(1).kind == TokenKind::ColonColon) {
        coutOffset = 2;
    }
    if (peek(coutOffset).kind == TokenKind::Identifier && peek(coutOffset).text == "cout" &&
        peek(coutOffset + 1).kind == TokenKind::LessLess) {
        SourceLocation loc = peek().loc;
        if (coutOffset > 0) { advance(); advance(); }
        advance(); // 'cout'
        std::vector<std::unique_ptr<Expr>> exprs;
        while (match(TokenKind::LessLess)) {
            int endlOffset = 0;
            if (check(TokenKind::Identifier) && peek().text == "std" && peek(1).kind == TokenKind::ColonColon) {
                endlOffset = 2;
            }
            if (peek(endlOffset).kind == TokenKind::Identifier && peek(endlOffset).text == "endl") {
                SourceLocation eloc = peek().loc;
                if (endlOffset > 0) { advance(); advance(); }
                advance(); // consume 'endl'
                exprs.push_back(std::make_unique<EndlExpr>(eloc));
            } else {
                exprs.push_back(parseAdditive());
            }
        }
        expect(TokenKind::Semicolon, "Expected ';' after cout statement");
        return std::make_unique<CoutStmt>(std::move(exprs), loc);
    }

    // cin >> ... or std::cin >> ...
    int cinOffset = 0;
    if (check(TokenKind::Identifier) && peek().text == "std" && peek(1).kind == TokenKind::ColonColon) {
        cinOffset = 2;
    }
    if (peek(cinOffset).kind == TokenKind::Identifier && peek(cinOffset).text == "cin" &&
        peek(cinOffset + 1).kind == TokenKind::GreaterGreater) {
        SourceLocation loc = peek().loc;
        if (cinOffset > 0) { advance(); advance(); }
        advance(); // 'cin'
        std::vector<std::unique_ptr<Expr>> targets;
        while (match(TokenKind::GreaterGreater)) {
            targets.push_back(parseUnary());
        }
        expect(TokenKind::Semicolon, "Expected ';' after cin statement");
        return std::make_unique<CinStmt>(std::move(targets), loc);
    }

    // Check if variable declaration: type identifier ...
    // Note: If peek is a built-in type keyword (int, char, bool, void) or an identifier followed by identifier or star
    bool isDecl = false;
    if (peek().kind == TokenKind::KwInt || peek().kind == TokenKind::KwChar ||
        peek().kind == TokenKind::KwBool || peek().kind == TokenKind::KwVoid) {
        isDecl = true;
    } else if (peek().kind == TokenKind::Identifier) {
        // Lookahead to see if next token is star or identifier (e.g. MyClass c; or MyClass* p;)
        int look = 1;
        while (peek(look).kind == TokenKind::Star) {
            look++;
        }
        if (peek(look).kind == TokenKind::Identifier) {
            isDecl = true;
        }
    }

    if (isDecl) {
        return parseVarDeclStatement();
    }

    // Expression statement
    SourceLocation loc = peek().loc;
    auto expr = parseExpression();
    expect(TokenKind::Semicolon, "Expected ';' after expression");
    return std::make_unique<ExprStmt>(std::move(expr), loc);
}

std::unique_ptr<BlockStmt> Parser::parseBlockStatement() {
    SourceLocation loc = expect(TokenKind::LBrace, "Expected '{'").loc;
    std::vector<std::unique_ptr<Stmt>> stmts;

    while (!check(TokenKind::RBrace) && !isAtEnd()) {
        stmts.push_back(parseStatement());
    }

    expect(TokenKind::RBrace, "Expected '}'");
    return std::make_unique<BlockStmt>(std::move(stmts), loc);
}

std::unique_ptr<Stmt> Parser::parseVarDeclStatement() {
    SourceLocation loc = peek().loc;
    ParsedType baseType = parseType();
    std::vector<std::unique_ptr<VarDeclStmt>> decls;

    while (true) {
        ParsedType varType = baseType;
        while (match(TokenKind::Star)) {
            varType.pointerDepth++;
        }

        const Token& nameTok = expect(TokenKind::Identifier, "Expected variable name");

        if (match(TokenKind::LBracket)) {
            varType.isArray = true;
            const Token& sizeTok = expect(TokenKind::IntLiteral, "Expected integer array size");
            varType.arraySize = sizeTok.intValue;
            expect(TokenKind::RBracket, "Expected ']' after array size");
        }

        std::unique_ptr<Expr> init = nullptr;
        if (match(TokenKind::Equal)) {
            init = parseExpression();
        }
        decls.push_back(std::make_unique<VarDeclStmt>(varType, nameTok.text, std::move(init), loc));

        if (!match(TokenKind::Comma)) {
            break;
        }
    }

    expect(TokenKind::Semicolon, "Expected ';' after variable declaration");
    if (decls.size() == 1) {
        return std::move(decls[0]);
    }
    return std::make_unique<DeclGroupStmt>(std::move(decls), loc);
}

std::unique_ptr<Stmt> Parser::parseIfStatement() {
    SourceLocation loc = advance().loc; // 'if'
    expect(TokenKind::LParen, "Expected '(' after 'if'");
    auto cond = parseExpression();
    expect(TokenKind::RParen, "Expected ')' after condition");
    auto thenBranch = parseStatement();
    std::unique_ptr<Stmt> elseBranch = nullptr;
    if (match(TokenKind::KwElse)) {
        elseBranch = parseStatement();
    }
    return std::make_unique<IfStmt>(std::move(cond), std::move(thenBranch), std::move(elseBranch), loc);
}

std::unique_ptr<Stmt> Parser::parseWhileStatement() {
    SourceLocation loc = advance().loc; // 'while'
    expect(TokenKind::LParen, "Expected '(' after 'while'");
    auto cond = parseExpression();
    expect(TokenKind::RParen, "Expected ')' after condition");
    auto body = parseStatement();
    return std::make_unique<WhileStmt>(std::move(cond), std::move(body), loc);
}

std::unique_ptr<Stmt> Parser::parseForStatement() {
    SourceLocation loc = advance().loc; // 'for'
    expect(TokenKind::LParen, "Expected '(' after 'for'");

    std::unique_ptr<Stmt> init = nullptr;
    if (!match(TokenKind::Semicolon)) {
        if (peek().kind == TokenKind::KwInt || peek().kind == TokenKind::KwChar ||
            peek().kind == TokenKind::KwBool) {
            init = parseVarDeclStatement();
        } else {
            auto expr = parseExpression();
            expect(TokenKind::Semicolon, "Expected ';' after for init expression");
            init = std::make_unique<ExprStmt>(std::move(expr), loc);
        }
    }

    std::unique_ptr<Expr> cond = nullptr;
    if (!match(TokenKind::Semicolon)) {
        cond = parseExpression();
        expect(TokenKind::Semicolon, "Expected ';' after for condition");
    }

    std::unique_ptr<Expr> step = nullptr;
    if (!check(TokenKind::RParen)) {
        step = parseExpression();
    }
    expect(TokenKind::RParen, "Expected ')' after for clauses");

    auto body = parseStatement();
    return std::make_unique<ForStmt>(std::move(init), std::move(cond), std::move(step), std::move(body), loc);
}

std::unique_ptr<Stmt> Parser::parseReturnStatement() {
    SourceLocation loc = advance().loc; // 'return'
    std::unique_ptr<Expr> val = nullptr;
    if (!match(TokenKind::Semicolon)) {
        val = parseExpression();
        expect(TokenKind::Semicolon, "Expected ';' after return value");
    }
    return std::make_unique<ReturnStmt>(std::move(val), loc);
}

// Expression parsing
std::unique_ptr<Expr> Parser::parseExpression() {
    return parseAssignment();
}

std::unique_ptr<Expr> Parser::parseAssignment() {
    auto expr = parseLogicalOr();

    if (check(TokenKind::Equal) || check(TokenKind::PlusEqual) || check(TokenKind::MinusEqual) ||
        check(TokenKind::StarEqual) || check(TokenKind::SlashEqual)) {
        const Token& opTok = advance();
        AssignOp op = AssignOp::Assign;
        if (opTok.kind == TokenKind::PlusEqual) op = AssignOp::AddAssign;
        else if (opTok.kind == TokenKind::MinusEqual) op = AssignOp::SubAssign;
        else if (opTok.kind == TokenKind::StarEqual) op = AssignOp::MulAssign;
        else if (opTok.kind == TokenKind::SlashEqual) op = AssignOp::DivAssign;

        auto right = parseAssignment(); // Right-associative
        return std::make_unique<AssignExpr>(op, std::move(expr), std::move(right), opTok.loc);
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenKind::PipePipe)) {
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryExpr>(BinaryOp::LogOr, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto left = parseBitwiseOr();
    while (match(TokenKind::AmpAmp)) {
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseBitwiseOr();
        left = std::make_unique<BinaryExpr>(BinaryOp::LogAnd, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseBitwiseOr() {
    auto left = parseBitwiseXor();
    while (match(TokenKind::Pipe)) {
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseBitwiseXor();
        left = std::make_unique<BinaryExpr>(BinaryOp::BitOr, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseBitwiseXor() {
    auto left = parseBitwiseAnd();
    while (match(TokenKind::Caret)) {
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseBitwiseAnd();
        left = std::make_unique<BinaryExpr>(BinaryOp::BitXor, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseBitwiseAnd() {
    auto left = parseEquality();
    while (match(TokenKind::Amp)) {
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseEquality();
        left = std::make_unique<BinaryExpr>(BinaryOp::BitAnd, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto left = parseRelational();
    while (check(TokenKind::EqualEqual) || check(TokenKind::ExclaimEqual)) {
        TokenKind k = advance().kind;
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseRelational();
        BinaryOp op = (k == TokenKind::EqualEqual) ? BinaryOp::Eq : BinaryOp::Ne;
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseRelational() {
    auto left = parseShift();
    while (check(TokenKind::Less) || check(TokenKind::LessEqual) ||
           check(TokenKind::Greater) || check(TokenKind::GreaterEqual)) {
        TokenKind k = advance().kind;
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseShift();
        BinaryOp op = BinaryOp::Lt;
        if (k == TokenKind::Less) op = BinaryOp::Lt;
        else if (k == TokenKind::LessEqual) op = BinaryOp::Le;
        else if (k == TokenKind::Greater) op = BinaryOp::Gt;
        else if (k == TokenKind::GreaterEqual) op = BinaryOp::Ge;
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseShift() {
    auto left = parseAdditive();
    while (check(TokenKind::LessLess) || check(TokenKind::GreaterGreater)) {
        TokenKind k = advance().kind;
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseAdditive();
        BinaryOp op = (k == TokenKind::LessLess) ? BinaryOp::Shl : BinaryOp::Shr;
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        TokenKind k = advance().kind;
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseMultiplicative();
        BinaryOp op = (k == TokenKind::Plus) ? BinaryOp::Add : BinaryOp::Sub;
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
        TokenKind k = advance().kind;
        SourceLocation loc = tokens[cursor - 1].loc;
        auto right = parseUnary();
        BinaryOp op = BinaryOp::Mul;
        if (k == TokenKind::Star) op = BinaryOp::Mul;
        else if (k == TokenKind::Slash) op = BinaryOp::Div;
        else if (k == TokenKind::Percent) op = BinaryOp::Mod;
        left = std::make_unique<BinaryExpr>(op, std::move(left), std::move(right), loc);
    }
    return left;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    SourceLocation loc = peek().loc;

    if (match(TokenKind::PlusPlus)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::PreInc, std::move(operand), loc);
    }
    if (match(TokenKind::MinusMinus)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::PreDec, std::move(operand), loc);
    }
    if (match(TokenKind::Plus)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::Plus, std::move(operand), loc);
    }
    if (match(TokenKind::Minus)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::Minus, std::move(operand), loc);
    }
    if (match(TokenKind::Exclaim)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::Not, std::move(operand), loc);
    }
    if (match(TokenKind::Tilde)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::BitNot, std::move(operand), loc);
    }
    if (match(TokenKind::Amp)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::AddressOf, std::move(operand), loc);
    }
    if (match(TokenKind::Star)) {
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>(UnaryOp::Deref, std::move(operand), loc);
    }

    if (match(TokenKind::KwSizeof)) {
        expect(TokenKind::LParen, "Expected '(' after sizeof");
        if (check(TokenKind::KwInt) || check(TokenKind::KwChar) ||
            check(TokenKind::KwBool) || check(TokenKind::KwVoid)) {
            ParsedType t = parseType();
            expect(TokenKind::RParen, "Expected ')' after sizeof");
            return std::make_unique<SizeofExpr>(t, loc);
        } else {
            auto inner = parseExpression();
            expect(TokenKind::RParen, "Expected ')' after sizeof");
            return std::make_unique<SizeofExpr>(std::move(inner), loc);
        }
    }

    if (match(TokenKind::KwNew)) {
        ParsedType allocType = parseType();
        std::vector<std::unique_ptr<Expr>> args;
        std::unique_ptr<Expr> arrSize = nullptr;

        if (match(TokenKind::LBracket)) {
            arrSize = parseExpression();
            expect(TokenKind::RBracket, "Expected ']' after array size in new");
            allocType.isArray = true;
        } else if (match(TokenKind::LParen)) {
            while (!check(TokenKind::RParen) && !isAtEnd()) {
                args.push_back(parseExpression());
                if (!match(TokenKind::Comma)) break;
            }
            expect(TokenKind::RParen, "Expected ')' after constructor args");
        }
        return std::make_unique<NewExpr>(allocType, std::move(args), std::move(arrSize), loc);
    }

    return parsePostfix();
}

std::unique_ptr<Expr> Parser::parsePostfix() {
    auto expr = parsePrimary();

    while (true) {
        SourceLocation loc = peek().loc;
        if (match(TokenKind::Dot) || match(TokenKind::Arrow)) {
            bool isArrow = (tokens[cursor - 1].kind == TokenKind::Arrow);
            const Token& memberTok = expect(TokenKind::Identifier, "Expected member name");
            if (match(TokenKind::LParen)) {
                // Method call: obj.method(args) or ptr->method(args)
                std::vector<std::unique_ptr<Expr>> args;
                while (!check(TokenKind::RParen) && !isAtEnd()) {
                    args.push_back(parseExpression());
                    if (!match(TokenKind::Comma)) break;
                }
                expect(TokenKind::RParen, "Expected ')' after method arguments");
                expr = std::make_unique<MethodCallExpr>(std::move(expr), memberTok.text, isArrow, std::move(args), loc);
            } else {
                // Member variable access: obj.field or ptr->field
                expr = std::make_unique<MemberExpr>(std::move(expr), memberTok.text, isArrow, loc);
            }
        } else if (match(TokenKind::LBracket)) {
            auto index = parseExpression();
            expect(TokenKind::RBracket, "Expected ']' after index");
            expr = std::make_unique<IndexExpr>(std::move(expr), std::move(index), loc);
        } else if (match(TokenKind::PlusPlus)) {
            expr = std::make_unique<UnaryExpr>(UnaryOp::PostInc, std::move(expr), loc);
        } else if (match(TokenKind::MinusMinus)) {
            expr = std::make_unique<UnaryExpr>(UnaryOp::PostDec, std::move(expr), loc);
        } else {
            break;
        }
    }

    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    SourceLocation loc = peek().loc;

    if (match(TokenKind::IntLiteral)) {
        return std::make_unique<IntLiteralExpr>(tokens[cursor - 1].intValue, loc);
    }
    if (match(TokenKind::CharLiteral)) {
        return std::make_unique<CharLiteralExpr>(tokens[cursor - 1].charValue, loc);
    }
    if (match(TokenKind::StringLiteral)) {
        return std::make_unique<StringLiteralExpr>(tokens[cursor - 1].text, loc);
    }
    if (match(TokenKind::BoolLiteral)) {
        return std::make_unique<BoolLiteralExpr>(tokens[cursor - 1].boolValue, loc);
    }
    if (match(TokenKind::KwNullptr)) {
        return std::make_unique<NullptrExpr>(loc);
    }
    if (match(TokenKind::KwThis)) {
        return std::make_unique<ThisExpr>(loc);
    }

    // Skip std:: if present
    if (check(TokenKind::Identifier) && peek().text == "std" && peek(1).kind == TokenKind::ColonColon) {
        advance(); advance();
    }

    if (match(TokenKind::Identifier)) {
        std::string name = tokens[cursor - 1].text;
        if (name == "endl") {
            return std::make_unique<EndlExpr>(loc);
        }
        // Function call: name(args)
        if (match(TokenKind::LParen)) {
            std::vector<std::unique_ptr<Expr>> args;
            while (!check(TokenKind::RParen) && !isAtEnd()) {
                args.push_back(parseExpression());
                if (!match(TokenKind::Comma)) break;
            }
            expect(TokenKind::RParen, "Expected ')' after function arguments");
            return std::make_unique<CallExpr>(name, std::move(args), loc);
        }
        return std::make_unique<VariableExpr>(name, loc);
    }

    if (match(TokenKind::LParen)) {
        auto expr = parseExpression();
        expect(TokenKind::RParen, "Expected ')' after expression");
        return expr;
    }

    error("Unexpected token '" + peek().text + "' in expression", loc);
    advance(); // avoid infinite loop
    return std::make_unique<IntLiteralExpr>(0, loc);
}

} // namespace mcxx

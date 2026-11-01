#pragma once

#include "type.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

namespace mcxx {

enum class SymbolKind {
    LocalVar,
    Parameter,
    GlobalVar,
    Field
};

struct Symbol {
    std::string name;
    std::shared_ptr<Type> type;
    SymbolKind kind;
    int offset = 0; // stack offset from %ebp for local/param, or field offset for struct
    std::string globalLabel; // assembly label for global
};

struct FunctionSymbol {
    std::string name;
    std::string mangledName;
    std::shared_ptr<Type> returnType;
    std::vector<std::shared_ptr<Type>> paramTypes;
    std::vector<std::string> paramNames;
    bool isMethod = false;
    std::string parentClassName;
    bool isExtern = false;
};

class Scope {
public:
    explicit Scope(std::shared_ptr<Scope> parent = nullptr) : parent(parent) {}

    bool declare(const std::string& name, const Symbol& sym) {
        if (symbols.find(name) != symbols.end()) {
            return false; // already declared in current scope
        }
        symbols[name] = sym;
        return true;
    }

    Symbol* lookup(const std::string& name) {
        auto it = symbols.find(name);
        if (it != symbols.end()) {
            return &it->second;
        }
        if (parent) {
            return parent->lookup(name);
        }
        return nullptr;
    }

    std::shared_ptr<Scope> getParent() const { return parent; }

private:
    std::shared_ptr<Scope> parent;
    std::unordered_map<std::string, Symbol> symbols;
};

class SymbolTable {
public:
    SymbolTable() {
        currentScope = std::make_shared<Scope>(nullptr);
        globalScope = currentScope;
    }

    void enterScope() {
        currentScope = std::make_shared<Scope>(currentScope);
    }

    void exitScope() {
        if (currentScope->getParent()) {
            currentScope = currentScope->getParent();
        }
    }

    bool declareVar(const std::string& name, const Symbol& sym) {
        return currentScope->declare(name, sym);
    }

    Symbol* lookupVar(const std::string& name) {
        return currentScope->lookup(name);
    }

    bool declareFunction(const std::string& name, const FunctionSymbol& fn) {
        if (functions.find(name) != functions.end()) {
            return false;
        }
        functions[name] = fn;
        return true;
    }

    FunctionSymbol* lookupFunction(const std::string& name) {
        auto it = functions.find(name);
        if (it != functions.end()) {
            return &it->second;
        }
        return nullptr;
    }

    bool declareClass(const std::string& name, std::shared_ptr<Type> classType) {
        if (classes.find(name) != classes.end()) {
            return false;
        }
        classes[name] = classType;
        return true;
    }

    std::shared_ptr<Type> lookupClass(const std::string& name) {
        auto it = classes.find(name);
        if (it != classes.end()) {
            return it->second;
        }
        return nullptr;
    }

    const std::unordered_map<std::string, FunctionSymbol>& getAllFunctions() const {
        return functions;
    }

    const std::unordered_map<std::string, std::shared_ptr<Type>>& getAllClasses() const {
        return classes;
    }

private:
    std::shared_ptr<Scope> currentScope;
    std::shared_ptr<Scope> globalScope;
    std::unordered_map<std::string, FunctionSymbol> functions;
    std::unordered_map<std::string, std::shared_ptr<Type>> classes;
};

} // namespace mcxx

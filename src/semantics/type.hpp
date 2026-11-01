#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>

namespace mcxx {

enum class TypeTag {
    Int,
    Char,
    Bool,
    Void,
    Pointer,
    Array,
    Class
};

struct FieldInfo {
    std::string name;
    std::shared_ptr<class Type> type;
    int offset = 0;
};

class Type {
public:
    TypeTag tag;
    std::string className; // if tag == Class
    std::shared_ptr<Type> pointeeType; // if tag == Pointer or Array
    int arraySize = 0; // if tag == Array

    // For Class/Struct types:
    std::vector<FieldInfo> fields;
    std::unordered_map<std::string, int> fieldIndex;
    int classSize = 0;

    explicit Type(TypeTag tag) : tag(tag) {}

    bool isInt() const { return tag == TypeTag::Int; }
    bool isChar() const { return tag == TypeTag::Char; }
    bool isBool() const { return tag == TypeTag::Bool; }
    bool isVoid() const { return tag == TypeTag::Void; }
    bool isPointer() const { return tag == TypeTag::Pointer; }
    bool isArray() const { return tag == TypeTag::Array; }
    bool isClass() const { return tag == TypeTag::Class; }

    bool isInteger() const {
        return tag == TypeTag::Int || tag == TypeTag::Char || tag == TypeTag::Bool;
    }

    int size() const {
        switch (tag) {
            case TypeTag::Int: return 4;
            case TypeTag::Char: return 1;
            case TypeTag::Bool: return 1;
            case TypeTag::Void: return 0;
            case TypeTag::Pointer: return 4; // 32-bit x86 pointer
            case TypeTag::Array: return arraySize * (pointeeType ? pointeeType->size() : 4);
            case TypeTag::Class: return classSize;
        }
        return 4;
    }

    int alignment() const {
        switch (tag) {
            case TypeTag::Char:
            case TypeTag::Bool: return 1;
            case TypeTag::Int:
            case TypeTag::Pointer: return 4;
            case TypeTag::Array: return pointeeType ? pointeeType->alignment() : 4;
            case TypeTag::Class: return 4;
            default: return 4;
        }
    }

    std::string toString() const {
        switch (tag) {
            case TypeTag::Int: return "int";
            case TypeTag::Char: return "char";
            case TypeTag::Bool: return "bool";
            case TypeTag::Void: return "void";
            case TypeTag::Pointer: return (pointeeType ? pointeeType->toString() : "void") + "*";
            case TypeTag::Array: return (pointeeType ? pointeeType->toString() : "void") + "[" + std::to_string(arraySize) + "]";
            case TypeTag::Class: return className;
        }
        return "unknown";
    }

    static std::shared_ptr<Type> makeInt() { return std::make_shared<Type>(TypeTag::Int); }
    static std::shared_ptr<Type> makeChar() { return std::make_shared<Type>(TypeTag::Char); }
    static std::shared_ptr<Type> makeBool() { return std::make_shared<Type>(TypeTag::Bool); }
    static std::shared_ptr<Type> makeVoid() { return std::make_shared<Type>(TypeTag::Void); }

    static std::shared_ptr<Type> makePointer(std::shared_ptr<Type> pointee) {
        auto t = std::make_shared<Type>(TypeTag::Pointer);
        t->pointeeType = std::move(pointee);
        return t;
    }

    static std::shared_ptr<Type> makeArray(std::shared_ptr<Type> elem, int count) {
        auto t = std::make_shared<Type>(TypeTag::Array);
        t->pointeeType = std::move(elem);
        t->arraySize = count;
        return t;
    }

    static std::shared_ptr<Type> makeClass(std::string name) {
        auto t = std::make_shared<Type>(TypeTag::Class);
        t->className = std::move(name);
        return t;
    }

    bool equals(const Type& other) const {
        if (tag != other.tag) return false;
        if (tag == TypeTag::Pointer) {
            return pointeeType->equals(*other.pointeeType);
        }
        if (tag == TypeTag::Array) {
            return arraySize == other.arraySize && pointeeType->equals(*other.pointeeType);
        }
        if (tag == TypeTag::Class) {
            return className == other.className;
        }
        return true;
    }
};

} // namespace mcxx

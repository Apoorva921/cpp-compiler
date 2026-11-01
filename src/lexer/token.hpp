#pragma once

#include <string>
#include <iostream>

namespace mcxx {

enum class TokenKind {
    // End of file & special
    Eof,
    Invalid,

    // Identifiers & Literals
    Identifier,
    IntLiteral,
    CharLiteral,
    StringLiteral,
    BoolLiteral,

    // Keywords
    KwInt,
    KwChar,
    KwBool,
    KwVoid,
    KwIf,
    KwElse,
    KwWhile,
    KwFor,
    KwReturn,
    KwBreak,
    KwContinue,
    KwStruct,
    KwClass,
    KwPublic,
    KwPrivate,
    KwNew,
    KwDelete,
    KwNullptr,
    KwThis,
    KwExtern,
    KwSizeof,

    // Operators
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    PlusPlus,       // ++
    MinusMinus,     // --
    Equal,          // =
    PlusEqual,      // +=
    MinusEqual,     // -=
    StarEqual,      // *=
    SlashEqual,     // /=
    EqualEqual,     // ==
    ExclaimEqual,   // !=
    Less,           // <
    LessEqual,      // <=
    Greater,        // >
    GreaterEqual,   // >=
    AmpAmp,         // &&
    PipePipe,       // ||
    Exclaim,        // !
    Amp,            // &
    Pipe,           // |
    Caret,          // ^
    Tilde,          // ~
    LessLess,       // <<
    GreaterGreater, // >>
    Arrow,          // ->
    Dot,            // .
    ColonColon,     // ::
    Question,       // ?

    // Delimiters
    LParen,         // (
    RParen,         // )
    LBracket,       // [
    RBracket,       // ]
    LBrace,         // {
    RBrace,         // }
    Semicolon,      // ;
    Comma,          // ,
    Colon           // :
};

struct SourceLocation {
    std::string filename;
    int line = 1;
    int column = 1;
};

struct Token {
    TokenKind kind = TokenKind::Invalid;
    std::string text;
    int intValue = 0;
    char charValue = 0;
    bool boolValue = false;
    SourceLocation loc;

    std::string toString() const;
};

const char* tokenKindToString(TokenKind kind);

} // namespace mcxx

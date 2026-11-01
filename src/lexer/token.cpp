#include "token.hpp"
#include <sstream>

namespace mcxx {

const char* tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TokenKind::Eof: return "EOF";
        case TokenKind::Invalid: return "INVALID";
        case TokenKind::Identifier: return "IDENTIFIER";
        case TokenKind::IntLiteral: return "INT_LITERAL";
        case TokenKind::CharLiteral: return "CHAR_LITERAL";
        case TokenKind::StringLiteral: return "STRING_LITERAL";
        case TokenKind::BoolLiteral: return "BOOL_LITERAL";
        case TokenKind::KwInt: return "int";
        case TokenKind::KwChar: return "char";
        case TokenKind::KwBool: return "bool";
        case TokenKind::KwVoid: return "void";
        case TokenKind::KwIf: return "if";
        case TokenKind::KwElse: return "else";
        case TokenKind::KwWhile: return "while";
        case TokenKind::KwFor: return "for";
        case TokenKind::KwReturn: return "return";
        case TokenKind::KwBreak: return "break";
        case TokenKind::KwContinue: return "continue";
        case TokenKind::KwStruct: return "struct";
        case TokenKind::KwClass: return "class";
        case TokenKind::KwPublic: return "public";
        case TokenKind::KwPrivate: return "private";
        case TokenKind::KwNew: return "new";
        case TokenKind::KwDelete: return "delete";
        case TokenKind::KwNullptr: return "nullptr";
        case TokenKind::KwThis: return "this";
        case TokenKind::KwExtern: return "extern";
        case TokenKind::KwSizeof: return "sizeof";
        case TokenKind::Plus: return "+";
        case TokenKind::Minus: return "-";
        case TokenKind::Star: return "*";
        case TokenKind::Slash: return "/";
        case TokenKind::Percent: return "%";
        case TokenKind::PlusPlus: return "++";
        case TokenKind::MinusMinus: return "--";
        case TokenKind::Equal: return "=";
        case TokenKind::PlusEqual: return "+=";
        case TokenKind::MinusEqual: return "-=";
        case TokenKind::StarEqual: return "*=";
        case TokenKind::SlashEqual: return "/=";
        case TokenKind::EqualEqual: return "==";
        case TokenKind::ExclaimEqual: return "!=";
        case TokenKind::Less: return "<";
        case TokenKind::LessEqual: return "<=";
        case TokenKind::Greater: return ">";
        case TokenKind::GreaterEqual: return ">=";
        case TokenKind::AmpAmp: return "&&";
        case TokenKind::PipePipe: return "||";
        case TokenKind::Exclaim: return "!";
        case TokenKind::Amp: return "&";
        case TokenKind::Pipe: return "|";
        case TokenKind::Caret: return "^";
        case TokenKind::Tilde: return "~";
        case TokenKind::LessLess: return "<<";
        case TokenKind::GreaterGreater: return ">>";
        case TokenKind::Arrow: return "->";
        case TokenKind::Dot: return ".";
        case TokenKind::ColonColon: return "::";
        case TokenKind::Question: return "?";
        case TokenKind::LParen: return "(";
        case TokenKind::RParen: return ")";
        case TokenKind::LBracket: return "[";
        case TokenKind::RBracket: return "]";
        case TokenKind::LBrace: return "{";
        case TokenKind::RBrace: return "}";
        case TokenKind::Semicolon: return ";";
        case TokenKind::Comma: return ",";
        case TokenKind::Colon: return ":";
        default: return "UNKNOWN";
    }
}

std::string Token::toString() const {
    std::ostringstream oss;
    oss << loc.line << ":" << loc.column << " [" << tokenKindToString(kind) << "]";
    if (!text.empty()) {
        oss << " '" << text << "'";
    }
    return oss.str();
}

} // namespace mcxx

#include "lexer.hpp"
#include <cctype>
#include <unordered_map>
#include <stdexcept>
#include <sstream>

namespace mcxx {

static const std::unordered_map<std::string, TokenKind> keywords = {
    {"int", TokenKind::KwInt},
    {"char", TokenKind::KwChar},
    {"bool", TokenKind::KwBool},
    {"void", TokenKind::KwVoid},
    {"if", TokenKind::KwIf},
    {"else", TokenKind::KwElse},
    {"while", TokenKind::KwWhile},
    {"for", TokenKind::KwFor},
    {"return", TokenKind::KwReturn},
    {"break", TokenKind::KwBreak},
    {"continue", TokenKind::KwContinue},
    {"struct", TokenKind::KwStruct},
    {"class", TokenKind::KwClass},
    {"public", TokenKind::KwPublic},
    {"private", TokenKind::KwPrivate},
    {"new", TokenKind::KwNew},
    {"delete", TokenKind::KwDelete},
    {"nullptr", TokenKind::KwNullptr},
    {"this", TokenKind::KwThis},
    {"extern", TokenKind::KwExtern},
    {"sizeof", TokenKind::KwSizeof},
    {"true", TokenKind::BoolLiteral},
    {"false", TokenKind::BoolLiteral}
};

Lexer::Lexer(std::string source, std::string filename)
    : source(std::move(source)), filename(std::move(filename)), cursor(0), line(1), column(1) {}

char Lexer::peek(int offset) const {
    if (cursor + offset >= source.size()) return '\0';
    return source[cursor + offset];
}

char Lexer::advance() {
    if (isAtEnd()) return '\0';
    char c = source[cursor++];
    if (c == '\n') {
        line++;
        column = 1;
    } else {
        column++;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source[cursor] != expected) return false;
    advance();
    return true;
}

bool Lexer::isAtEnd() const {
    return cursor >= source.size();
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
        } else if (c == '/' && peek(1) == '/') {
            // Line comment
            advance(); advance();
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else if (c == '/' && peek(1) == '*') {
            // Block comment
            advance(); advance();
            while (!isAtEnd()) {
                if (peek() == '*' && peek(1) == '/') {
                    advance(); advance();
                    break;
                }
                advance();
            }
        } else if (c == '#') {
            // Preprocessor directive: skip until newline
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
        } else {
            break;
        }
    }
}

char Lexer::parseEscapeSequence() {
    // advance past '\'
    char c = advance();
    switch (c) {
        case 'n': return '\n';
        case 't': return '\t';
        case 'r': return '\r';
        case '\\': return '\\';
        case '\'': return '\'';
        case '"': return '\"';
        case '0': return '\0';
        default: return c;
    }
}

Token Lexer::lexNumber() {
    SourceLocation loc{filename, line, column};
    std::string text;
    bool isHex = false;

    if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
        isHex = true;
        text += advance();
        text += advance();
        while (std::isxdigit(static_cast<unsigned>(peek()))) {
            text += advance();
        }
    } else {
        while (std::isdigit(static_cast<unsigned>(peek()))) {
            text += advance();
        }
    }

    int value = 0;
    try {
        value = std::stoi(text, nullptr, isHex ? 16 : 10);
    } catch (...) {
        value = 0;
    }

    Token tok;
    tok.kind = TokenKind::IntLiteral;
    tok.text = text;
    tok.intValue = value;
    tok.loc = loc;
    return tok;
}

Token Lexer::lexIdentifierOrKeyword() {
    SourceLocation loc{filename, line, column};
    std::string text;

    while (std::isalnum(static_cast<unsigned>(peek())) || peek() == '_') {
        text += advance();
    }

    Token tok;
    tok.loc = loc;
    tok.text = text;

    auto it = keywords.find(text);
    if (it != keywords.end()) {
        tok.kind = it->second;
        if (tok.kind == TokenKind::BoolLiteral) {
            tok.boolValue = (text == "true");
        }
    } else {
        tok.kind = TokenKind::Identifier;
    }

    return tok;
}

Token Lexer::lexString() {
    SourceLocation loc{filename, line, column};
    advance(); // skip opening quote
    std::string result;

    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\\') {
            advance();
            result += parseEscapeSequence();
        } else {
            result += advance();
        }
    }

    if (!isAtEnd()) {
        advance(); // skip closing quote
    }

    Token tok;
    tok.kind = TokenKind::StringLiteral;
    tok.text = result;
    tok.loc = loc;
    return tok;
}

Token Lexer::lexChar() {
    SourceLocation loc{filename, line, column};
    advance(); // skip opening single quote
    char val = '\0';
    if (!isAtEnd() && peek() != '\'') {
        if (peek() == '\\') {
            advance();
            val = parseEscapeSequence();
        } else {
            val = advance();
        }
    }
    if (!isAtEnd() && peek() == '\'') {
        advance(); // skip closing single quote
    }

    Token tok;
    tok.kind = TokenKind::CharLiteral;
    tok.charValue = val;
    tok.intValue = static_cast<int>(val);
    tok.text = std::string(1, val);
    tok.loc = loc;
    return tok;
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (true) {
        skipWhitespaceAndComments();
        if (isAtEnd()) {
            Token eofTok;
            eofTok.kind = TokenKind::Eof;
            eofTok.loc = {filename, line, column};
            tokens.push_back(eofTok);
            break;
        }

        SourceLocation loc{filename, line, column};
        char c = peek();

        if (std::isdigit(static_cast<unsigned>(c))) {
            tokens.push_back(lexNumber());
            continue;
        }

        if (std::isalpha(static_cast<unsigned>(c)) || c == '_') {
            tokens.push_back(lexIdentifierOrKeyword());
            continue;
        }

        if (c == '"') {
            tokens.push_back(lexString());
            continue;
        }

        if (c == '\'') {
            tokens.push_back(lexChar());
            continue;
        }

        // Operators and Delimiters
        advance();
        Token tok;
        tok.loc = loc;

        switch (c) {
            case '+':
                if (match('+')) tok.kind = TokenKind::PlusPlus;
                else if (match('=')) tok.kind = TokenKind::PlusEqual;
                else tok.kind = TokenKind::Plus;
                break;
            case '-':
                if (match('-')) tok.kind = TokenKind::MinusMinus;
                else if (match('=')) tok.kind = TokenKind::MinusEqual;
                else if (match('>')) tok.kind = TokenKind::Arrow;
                else tok.kind = TokenKind::Minus;
                break;
            case '*':
                if (match('=')) tok.kind = TokenKind::StarEqual;
                else tok.kind = TokenKind::Star;
                break;
            case '/':
                if (match('=')) tok.kind = TokenKind::SlashEqual;
                else tok.kind = TokenKind::Slash;
                break;
            case '%':
                tok.kind = TokenKind::Percent;
                break;
            case '=':
                if (match('=')) tok.kind = TokenKind::EqualEqual;
                else tok.kind = TokenKind::Equal;
                break;
            case '!':
                if (match('=')) tok.kind = TokenKind::ExclaimEqual;
                else tok.kind = TokenKind::Exclaim;
                break;
            case '<':
                if (match('=')) tok.kind = TokenKind::LessEqual;
                else if (match('<')) tok.kind = TokenKind::LessLess;
                else tok.kind = TokenKind::Less;
                break;
            case '>':
                if (match('=')) tok.kind = TokenKind::GreaterEqual;
                else if (match('>')) tok.kind = TokenKind::GreaterGreater;
                else tok.kind = TokenKind::Greater;
                break;
            case '&':
                if (match('&')) tok.kind = TokenKind::AmpAmp;
                else tok.kind = TokenKind::Amp;
                break;
            case '|':
                if (match('|')) tok.kind = TokenKind::PipePipe;
                else tok.kind = TokenKind::Pipe;
                break;
            case '^': tok.kind = TokenKind::Caret; break;
            case '~': tok.kind = TokenKind::Tilde; break;
            case '.': tok.kind = TokenKind::Dot; break;
            case ':':
                if (match(':')) tok.kind = TokenKind::ColonColon;
                else tok.kind = TokenKind::Colon;
                break;
            case '?': tok.kind = TokenKind::Question; break;
            case '(': tok.kind = TokenKind::LParen; break;
            case ')': tok.kind = TokenKind::RParen; break;
            case '[': tok.kind = TokenKind::LBracket; break;
            case ']': tok.kind = TokenKind::RBracket; break;
            case '{': tok.kind = TokenKind::LBrace; break;
            case '}': tok.kind = TokenKind::RBrace; break;
            case ';': tok.kind = TokenKind::Semicolon; break;
            case ',': tok.kind = TokenKind::Comma; break;
            default:
                tok.kind = TokenKind::Invalid;
                tok.text = std::string(1, c);
                break;
        }

        if (tok.text.empty()) {
            tok.text = tokenKindToString(tok.kind);
        }
        tokens.push_back(tok);
    }

    return tokens;
}

} // namespace mcxx

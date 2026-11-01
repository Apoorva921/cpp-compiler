#pragma once

#include "token.hpp"
#include <string>
#include <vector>

namespace mcxx {

class Lexer {
public:
    Lexer(std::string source, std::string filename = "<stdin>");

    std::vector<Token> tokenize();

private:
    char peek(int offset = 0) const;
    char advance();
    bool match(char expected);
    bool isAtEnd() const;

    void skipWhitespaceAndComments();
    Token lexNumber();
    Token lexIdentifierOrKeyword();
    Token lexString();
    Token lexChar();

    char parseEscapeSequence();

    std::string source;
    std::string filename;
    size_t cursor = 0;
    int line = 1;
    int column = 1;
};

} // namespace mcxx

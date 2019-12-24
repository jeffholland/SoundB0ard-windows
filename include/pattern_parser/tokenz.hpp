#pragma once

#include <string>

namespace pattern_parser
{
using TokenType = std::string;

const TokenType PATTERN_COMMA = ",";
const TokenType PATTERN_DIVISOR = "/";
const TokenType PATTERN_EOF = "EOF";
const TokenType PATTERN_IDENT = "PATTERN_IDENT";
const TokenType PATTERN_ILLEGAL = "ILLEGAL";
const TokenType PATTERN_INT = "INT";
const TokenType PATTERN_MULTIPLIER = "*";
const TokenType PATTERN_SQUARE_BRACKET_LEFT = "[";
const TokenType PATTERN_SQUARE_BRACKET_RIGHT = "]";
const TokenType PATTERN_OPEN_PAREN = "(";
const TokenType PATTERN_CLOSE_PAREN = ")";
const TokenType PATTERN_OPEN_ANGLE_BRACKET = "<";
const TokenType PATTERN_CLOSE_ANGLE_BRACKET = ">";

class Token
{
  public:
    Token() {}
    Token(TokenType type, std::string literal)
        : type_{type}, literal_{literal} {};

    friend std::ostream &operator<<(std::ostream &, const Token &);

  public:
    TokenType type_;
    std::string literal_;
};

} // namespace pattern_parser

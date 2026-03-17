#pragma once

#include <string>
#include <vector>

#include "pinggen/token.hpp"

namespace pinggen {

class Lexer {
  public:
    explicit Lexer(std::string source, std::string source_name = "");
    std::vector<Token> tokenize();

  private:
    char peek(std::size_t offset = 0) const;
    char advance();
    bool match(char expected);
    void skip_whitespace();
    Token make_token(TokenKind kind, std::size_t start_line, std::size_t start_column, std::string lexeme);
    Token lex_identifier(std::size_t start_line, std::size_t start_column);
    Token lex_number(std::size_t start_line, std::size_t start_column);
    Token lex_string(std::size_t start_line, std::size_t start_column);

    std::string source_;
    std::string source_name_;
    std::size_t index_ = 0;
    std::size_t line_ = 1;
    std::size_t column_ = 1;
};

}  // namespace pinggen

#pragma once

#include <vector>

#include "pinggen/ast.hpp"

namespace pinggen {

class Parser {
  public:
    explicit Parser(std::vector<Token> tokens);
    Program parse();

  private:
    const Token& current() const;
    const Token& previous() const;
    bool is_at_end() const;
    bool check(TokenKind kind) const;
    bool check_next(TokenKind kind) const;
    bool match(TokenKind kind);
    const Token& consume(TokenKind kind, const std::string& message);

    ImportDecl parse_import();
    FunctionDecl parse_function();
    std::vector<std::unique_ptr<Stmt>> parse_block();
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Stmt> parse_let_statement();
    std::unique_ptr<Stmt> parse_return_statement();
    std::unique_ptr<Stmt> parse_if_statement();
    std::unique_ptr<Stmt> parse_while_statement();
    std::unique_ptr<Stmt> parse_assignment_or_expression_statement();
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::unique_ptr<Expr> parse_primary();
    ValueType parse_type();
    std::string parse_qualified_name();

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
};

}  // namespace pinggen

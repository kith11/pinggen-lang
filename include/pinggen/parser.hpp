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
    EnumDecl parse_enum();
    StructDecl parse_struct();
    FunctionDecl parse_function(const std::string& impl_target = "");
    std::vector<std::unique_ptr<Stmt>> parse_block();
    std::unique_ptr<Stmt> parse_statement();
    std::unique_ptr<Stmt> parse_let_statement();
    std::unique_ptr<Stmt> parse_return_statement();
    std::unique_ptr<Stmt> parse_if_statement();
    std::unique_ptr<Stmt> parse_while_statement();
    std::unique_ptr<Stmt> parse_for_statement();
    std::unique_ptr<Stmt> parse_break_statement();
    std::unique_ptr<Stmt> parse_continue_statement();
    std::unique_ptr<Stmt> parse_assignment_or_expression_statement();
    std::unique_ptr<Expr> parse_expression();
    std::unique_ptr<Expr> parse_or();
    std::unique_ptr<Expr> parse_and();
    std::unique_ptr<Expr> parse_comparison();
    std::unique_ptr<Expr> parse_equality();
    std::unique_ptr<Expr> parse_term();
    std::unique_ptr<Expr> parse_factor();
    std::unique_ptr<Expr> parse_unary();
    std::unique_ptr<Expr> parse_primary();
    std::unique_ptr<Expr> parse_postfix(std::unique_ptr<Expr> expr);
    std::unique_ptr<Expr> parse_for_bound_expression();
    Type parse_type();
    std::string parse_qualified_name();

    std::vector<Token> tokens_;
    std::size_t current_ = 0;
    bool parsing_for_bound_ = false;
};

}  // namespace pinggen

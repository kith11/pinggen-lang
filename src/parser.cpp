#include "pinggen/parser.hpp"

#include <utility>

#include "pinggen/diagnostics.hpp"

namespace pinggen {

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

Program Parser::parse() {
    Program program;
    while (check(TokenKind::KwImport)) {
        program.imports.push_back(parse_import());
    }
    while (!is_at_end()) {
        program.functions.push_back(parse_function());
    }
    return program;
}

const Token& Parser::current() const { return tokens_[current_]; }

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

bool Parser::is_at_end() const { return current().kind == TokenKind::EndOfFile; }

bool Parser::check(TokenKind kind) const { return current().kind == kind; }

bool Parser::check_next(TokenKind kind) const {
    if (current_ + 1 >= tokens_.size()) {
        return false;
    }
    return tokens_[current_ + 1].kind == kind;
}

bool Parser::match(TokenKind kind) {
    if (!check(kind)) {
        return false;
    }
    ++current_;
    return true;
}

const Token& Parser::consume(TokenKind kind, const std::string& message) {
    if (!check(kind)) {
        fail(current().location, message);
    }
    ++current_;
    return previous();
}

ImportDecl Parser::parse_import() {
    const Token import_token = consume(TokenKind::KwImport, "expected 'import'");
    consume(TokenKind::Identifier, "expected namespace name");
    consume(TokenKind::ColonColon, "expected '::' in import");
    consume(TokenKind::LBrace, "expected '{' in import");

    ImportDecl decl;
    decl.location = import_token.location;
    do {
        decl.items.push_back(consume(TokenKind::Identifier, "expected import item").lexeme);
    } while (match(TokenKind::Comma));
    consume(TokenKind::RBrace, "expected '}' after import list");
    return decl;
}

FunctionDecl Parser::parse_function() {
    const Token func_token = consume(TokenKind::KwFunc, "expected 'func'");
    FunctionDecl decl;
    decl.location = func_token.location;
    decl.name = consume(TokenKind::Identifier, "expected function name").lexeme;
    consume(TokenKind::LParen, "expected '(' after function name");
    consume(TokenKind::RParen, "expected ')' after function name");
    consume(TokenKind::LBrace, "expected '{' before function body");
    decl.body = parse_block();
    return decl;
}

std::vector<std::unique_ptr<Stmt>> Parser::parse_block() {
    std::vector<std::unique_ptr<Stmt>> body;
    while (!check(TokenKind::RBrace) && !is_at_end()) {
        body.push_back(parse_statement());
    }
    consume(TokenKind::RBrace, "expected '}' after block");
    return body;
}

std::unique_ptr<Stmt> Parser::parse_statement() {
    if (check(TokenKind::KwLet)) {
        return parse_let_statement();
    }
    if (check(TokenKind::KwReturn)) {
        return parse_return_statement();
    }
    return parse_assignment_or_expression_statement();
}

std::unique_ptr<Stmt> Parser::parse_let_statement() {
    const Token let_token = consume(TokenKind::KwLet, "expected 'let'");
    const bool is_mutable = match(TokenKind::KwMut);
    const std::string name = consume(TokenKind::Identifier, "expected variable name").lexeme;
    consume(TokenKind::Equal, "expected '=' after variable name");
    auto initializer = parse_expression();
    consume(TokenKind::Semicolon, "expected ';' after variable declaration");
    return std::make_unique<LetStmt>(let_token.location, name, is_mutable, std::move(initializer));
}

std::unique_ptr<Stmt> Parser::parse_return_statement() {
    const Token return_token = consume(TokenKind::KwReturn, "expected 'return'");
    if (match(TokenKind::Semicolon)) {
        return std::make_unique<ReturnStmt>(return_token.location, nullptr);
    }
    auto value = parse_expression();
    consume(TokenKind::Semicolon, "expected ';' after return statement");
    return std::make_unique<ReturnStmt>(return_token.location, std::move(value));
}

std::unique_ptr<Stmt> Parser::parse_assignment_or_expression_statement() {
    if (check(TokenKind::Identifier) && check_next(TokenKind::Equal)) {
        const Token name = consume(TokenKind::Identifier, "expected variable name");
        consume(TokenKind::Equal, "expected '=' in assignment");
        auto value = parse_expression();
        consume(TokenKind::Semicolon, "expected ';' after assignment");
        return std::make_unique<AssignStmt>(name.location, name.lexeme, std::move(value));
    }
    auto expr = parse_expression();
    consume(TokenKind::Semicolon, "expected ';' after expression");
    return std::make_unique<ExprStmt>(expr->location, std::move(expr));
}

std::unique_ptr<Expr> Parser::parse_expression() { return parse_term(); }

std::unique_ptr<Expr> Parser::parse_term() {
    auto expr = parse_factor();
    while (check(TokenKind::Plus) || check(TokenKind::Minus)) {
        const Token op = current();
        ++current_;
        auto right = parse_factor();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_factor() {
    auto expr = parse_primary();
    while (check(TokenKind::Star) || check(TokenKind::Slash)) {
        const Token op = current();
        ++current_;
        auto right = parse_primary();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (match(TokenKind::Integer)) {
        return std::make_unique<IntegerExpr>(previous().location, std::stoll(previous().lexeme));
    }
    if (match(TokenKind::String)) {
        return std::make_unique<StringExpr>(previous().location, previous().lexeme);
    }
    if (match(TokenKind::Identifier)) {
        const Token first = previous();
        if (check(TokenKind::ColonColon)) {
            --current_;
            const std::string name = parse_qualified_name();
            consume(TokenKind::LParen, "expected '(' after function name");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenKind::RParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RParen, "expected ')' after arguments");
            return std::make_unique<CallExpr>(first.location, name, std::move(args));
        }
        if (match(TokenKind::LParen)) {
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenKind::RParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RParen, "expected ')' after arguments");
            return std::make_unique<CallExpr>(first.location, first.lexeme, std::move(args));
        }
        return std::make_unique<VariableExpr>(first.location, first.lexeme);
    }
    if (match(TokenKind::LParen)) {
        auto expr = parse_expression();
        consume(TokenKind::RParen, "expected ')' after expression");
        return expr;
    }
    fail(current().location, "expected expression");
}

std::string Parser::parse_qualified_name() {
    std::string name = consume(TokenKind::Identifier, "expected name").lexeme;
    while (match(TokenKind::ColonColon)) {
        name += "::";
        name += consume(TokenKind::Identifier, "expected name after '::'").lexeme;
    }
    return name;
}

}  // namespace pinggen

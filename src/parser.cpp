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
        if (check(TokenKind::KwEnum)) {
            program.enums.push_back(parse_enum());
            continue;
        }
        if (check(TokenKind::KwStruct)) {
            program.structs.push_back(parse_struct());
            continue;
        }
        if (check(TokenKind::KwImpl)) {
            consume(TokenKind::KwImpl, "expected 'impl'");
            const std::string impl_target = consume(TokenKind::Identifier, "expected impl target type").lexeme;
            consume(TokenKind::LBrace, "expected '{' after impl target");
            while (!check(TokenKind::RBrace) && !is_at_end()) {
                program.functions.push_back(parse_function(impl_target));
            }
            consume(TokenKind::RBrace, "expected '}' after impl block");
            continue;
        }
        program.functions.push_back(parse_function(""));
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

EnumDecl Parser::parse_enum() {
    const Token enum_token = consume(TokenKind::KwEnum, "expected 'enum'");
    EnumDecl decl;
    decl.location = enum_token.location;
    decl.name = consume(TokenKind::Identifier, "expected enum name").lexeme;
    consume(TokenKind::LBrace, "expected '{' after enum name");
    while (!check(TokenKind::RBrace) && !is_at_end()) {
        EnumVariant variant;
        variant.location = current().location;
        variant.name = consume(TokenKind::Identifier, "expected variant name").lexeme;
        decl.variants.push_back(std::move(variant));
        match(TokenKind::Comma);
    }
    consume(TokenKind::RBrace, "expected '}' after enum body");
    return decl;
}

StructDecl Parser::parse_struct() {
    const Token struct_token = consume(TokenKind::KwStruct, "expected 'struct'");
    StructDecl decl;
    decl.location = struct_token.location;
    decl.name = consume(TokenKind::Identifier, "expected struct name").lexeme;
    consume(TokenKind::LBrace, "expected '{' after struct name");
    while (!check(TokenKind::RBrace) && !is_at_end()) {
        StructField field;
        field.location = current().location;
        field.name = consume(TokenKind::Identifier, "expected field name").lexeme;
        consume(TokenKind::Colon, "expected ':' after field name");
        field.type = parse_type();
        decl.fields.push_back(std::move(field));
        match(TokenKind::Comma);
    }
    consume(TokenKind::RBrace, "expected '}' after struct body");
    return decl;
}

FunctionDecl Parser::parse_function(const std::string& impl_target) {
    const Token func_token = consume(TokenKind::KwFunc, "expected 'func'");
    FunctionDecl decl;
    decl.location = func_token.location;
    decl.name = consume(TokenKind::Identifier, "expected function name").lexeme;
    decl.impl_target = impl_target;
    consume(TokenKind::LParen, "expected '(' after function name");
    if (!check(TokenKind::RParen)) {
        bool first_param = true;
        do {
            Parameter param;
            param.location = current().location;
            if (!impl_target.empty() && first_param && check(TokenKind::KwMut)) {
                consume(TokenKind::KwMut, "expected 'mut'");
                param.location = current().location;
                param.name = consume(TokenKind::Identifier, "expected receiver name").lexeme;
                if (param.name == "self") {
                    param.is_self = true;
                    param.is_mut_self = true;
                    param.type = Type::struct_type(impl_target);
                } else {
                    consume(TokenKind::Colon, "expected ':' after parameter name");
                    param.type = parse_type();
                }
            } else {
                param.name = consume(TokenKind::Identifier, "expected parameter name").lexeme;
                if (!impl_target.empty() && first_param && param.name == "self") {
                    param.is_self = true;
                    param.type = Type::struct_type(impl_target);
                } else {
                    consume(TokenKind::Colon, "expected ':' after parameter name");
                    param.type = parse_type();
                }
            }
            decl.params.push_back(std::move(param));
            first_param = false;
        } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "expected ')' after function parameters");
    if (match(TokenKind::Arrow)) {
        decl.return_type = parse_type();
    }
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
    if (check(TokenKind::KwIf)) {
        return parse_if_statement();
    }
    if (check(TokenKind::KwWhile)) {
        return parse_while_statement();
    }
    if (check(TokenKind::KwFor)) {
        return parse_for_statement();
    }
    if (check(TokenKind::KwBreak)) {
        return parse_break_statement();
    }
    if (check(TokenKind::KwContinue)) {
        return parse_continue_statement();
    }
    return parse_assignment_or_expression_statement();
}

std::unique_ptr<Stmt> Parser::parse_let_statement() {
    const Token let_token = consume(TokenKind::KwLet, "expected 'let'");
    const bool is_mutable = match(TokenKind::KwMut);
    const std::string name = consume(TokenKind::Identifier, "expected variable name").lexeme;
    std::optional<Type> declared_type;
    if (match(TokenKind::Colon)) {
        declared_type = parse_type();
    }
    consume(TokenKind::Equal, "expected '=' after variable name");
    auto initializer = parse_expression();
    consume(TokenKind::Semicolon, "expected ';' after variable declaration");
    return std::make_unique<LetStmt>(let_token.location, name, is_mutable, std::move(declared_type), std::move(initializer));
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

std::unique_ptr<Stmt> Parser::parse_if_statement() {
    const Token if_token = consume(TokenKind::KwIf, "expected 'if'");
    auto condition = parse_expression();
    consume(TokenKind::LBrace, "expected '{' before if block");
    auto then_body = parse_block();

    std::vector<std::unique_ptr<Stmt>> else_body;
    if (match(TokenKind::KwElse)) {
        if (check(TokenKind::LBrace)) {
            consume(TokenKind::LBrace, "expected '{' before else block");
            else_body = parse_block();
        } else {
            else_body.push_back(parse_statement());
        }
    }

    return std::make_unique<IfStmt>(if_token.location, std::move(condition), std::move(then_body), std::move(else_body));
}

std::unique_ptr<Stmt> Parser::parse_while_statement() {
    const Token while_token = consume(TokenKind::KwWhile, "expected 'while'");
    auto condition = parse_expression();
    consume(TokenKind::LBrace, "expected '{' before while block");
    auto loop_body = parse_block();
    return std::make_unique<WhileStmt>(while_token.location, std::move(condition), std::move(loop_body));
}

std::unique_ptr<Stmt> Parser::parse_for_statement() {
    const Token for_token = consume(TokenKind::KwFor, "expected 'for'");
    const std::string name = consume(TokenKind::Identifier, "expected loop variable name").lexeme;
    consume(TokenKind::KwIn, "expected 'in' after loop variable");
    auto start = parse_for_bound_expression();
    consume(TokenKind::DotDot, "expected '..' in for range");
    auto end = parse_for_bound_expression();
    consume(TokenKind::LBrace, "expected '{' before for block");
    auto loop_body = parse_block();
    return std::make_unique<ForStmt>(for_token.location, name, std::move(start), std::move(end), std::move(loop_body));
}

std::unique_ptr<Stmt> Parser::parse_break_statement() {
    const Token break_token = consume(TokenKind::KwBreak, "expected 'break'");
    consume(TokenKind::Semicolon, "expected ';' after break");
    return std::make_unique<BreakStmt>(break_token.location);
}

std::unique_ptr<Stmt> Parser::parse_continue_statement() {
    const Token continue_token = consume(TokenKind::KwContinue, "expected 'continue'");
    consume(TokenKind::Semicolon, "expected ';' after continue");
    return std::make_unique<ContinueStmt>(continue_token.location);
}

std::unique_ptr<Stmt> Parser::parse_assignment_or_expression_statement() {
    auto expr = parse_expression();
    if (match(TokenKind::Equal)) {
        auto value = parse_expression();
        const SourceLocation location = expr->location;
        consume(TokenKind::Semicolon, "expected ';' after assignment");

        if (const auto* variable = dynamic_cast<const VariableExpr*>(expr.get())) {
            return std::make_unique<AssignStmt>(location, variable->name, std::move(value));
        }
        if (auto* field = dynamic_cast<FieldAccessExpr*>(expr.get())) {
            auto owned = std::unique_ptr<FieldAccessExpr>(static_cast<FieldAccessExpr*>(expr.release()));
            return std::make_unique<FieldAssignStmt>(location, std::move(owned->object), owned->field, std::move(value));
        }
        if (auto* index = dynamic_cast<IndexExpr*>(expr.get())) {
            auto owned = std::unique_ptr<IndexExpr>(static_cast<IndexExpr*>(expr.release()));
            return std::make_unique<IndexAssignStmt>(
                location, std::move(owned->object), std::move(owned->index), std::move(value));
        }
        fail(location, "invalid assignment target");
    }
    consume(TokenKind::Semicolon, "expected ';' after expression");
    return std::make_unique<ExprStmt>(expr->location, std::move(expr));
}

std::unique_ptr<Expr> Parser::parse_expression() { return parse_or(); }

std::unique_ptr<Expr> Parser::parse_for_bound_expression() {
    const bool previous = parsing_for_bound_;
    parsing_for_bound_ = true;
    auto expr = parse_expression();
    parsing_for_bound_ = previous;
    return expr;
}

std::unique_ptr<Expr> Parser::parse_or() {
    auto expr = parse_and();
    while (check(TokenKind::OrOr)) {
        const Token op = current();
        ++current_;
        auto right = parse_and();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_and() {
    auto expr = parse_equality();
    while (check(TokenKind::AndAnd)) {
        const Token op = current();
        ++current_;
        auto right = parse_equality();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_equality() {
    auto expr = parse_comparison();
    while (check(TokenKind::EqualEqual) || check(TokenKind::BangEqual)) {
        const Token op = current();
        ++current_;
        auto right = parse_comparison();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_comparison() {
    auto expr = parse_term();
    while (check(TokenKind::Less) || check(TokenKind::LessEqual) || check(TokenKind::Greater) ||
           check(TokenKind::GreaterEqual)) {
        const Token op = current();
        ++current_;
        auto right = parse_term();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

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
    auto expr = parse_unary();
    while (check(TokenKind::Star) || check(TokenKind::Slash) || check(TokenKind::Percent)) {
        const Token op = current();
        ++current_;
        auto right = parse_unary();
        expr = std::make_unique<BinaryExpr>(op.location, op.lexeme, std::move(expr), std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parse_unary() {
    if (match(TokenKind::Bang)) {
        const Token op = previous();
        return std::make_unique<UnaryExpr>(op.location, op.lexeme, parse_unary());
    }
    return parse_primary();
}

std::unique_ptr<Expr> Parser::parse_primary() {
    if (match(TokenKind::Integer)) {
        return std::make_unique<IntegerExpr>(previous().location, std::stoll(previous().lexeme));
    }
    if (match(TokenKind::KwTrue)) {
        return std::make_unique<BoolExpr>(previous().location, true);
    }
    if (match(TokenKind::KwFalse)) {
        return std::make_unique<BoolExpr>(previous().location, false);
    }
    if (match(TokenKind::String)) {
        return std::make_unique<StringExpr>(previous().location, previous().lexeme);
    }
    if (match(TokenKind::LBracket)) {
        const SourceLocation location = previous().location;
        std::vector<std::unique_ptr<Expr>> elements;
        if (!check(TokenKind::RBracket)) {
            do {
                elements.push_back(parse_expression());
            } while (match(TokenKind::Comma));
        }
        consume(TokenKind::RBracket, "expected ']' after array literal");
        return parse_postfix(std::make_unique<ArrayLiteralExpr>(location, std::move(elements)));
    }
    if (match(TokenKind::Identifier)) {
        const Token first = previous();
        if (!parsing_for_bound_ && check(TokenKind::LBrace)) {
            ++current_;
            std::vector<StructLiteralField> fields;
            if (!check(TokenKind::RBrace)) {
                do {
                    StructLiteralField field;
                    field.location = current().location;
                    field.name = consume(TokenKind::Identifier, "expected field name").lexeme;
                    consume(TokenKind::Colon, "expected ':' after field name");
                    field.value = parse_expression();
                    fields.push_back(std::move(field));
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RBrace, "expected '}' after struct literal");
            return parse_postfix(std::make_unique<StructLiteralExpr>(first.location, first.lexeme, std::move(fields)));
        }
        if (check(TokenKind::ColonColon)) {
            consume(TokenKind::ColonColon, "expected '::'");
            const Token second = consume(TokenKind::Identifier, "expected name after '::'");
            std::string name = first.lexeme + "::" + second.lexeme;
            while (check(TokenKind::ColonColon) && check_next(TokenKind::Identifier)) {
                consume(TokenKind::ColonColon, "expected '::'");
                name += "::";
                name += consume(TokenKind::Identifier, "expected name after '::'").lexeme;
            }
            if (match(TokenKind::LParen)) {
                std::vector<std::unique_ptr<Expr>> args;
                if (!check(TokenKind::RParen)) {
                    do {
                        args.push_back(parse_expression());
                    } while (match(TokenKind::Comma));
                }
                consume(TokenKind::RParen, "expected ')' after arguments");
                return parse_postfix(std::make_unique<CallExpr>(first.location, name, std::move(args)));
            }
            if (name.find("::", name.find("::") + 2) != std::string::npos) {
                fail(first.location, "qualified values only support 'Type::Variant'");
            }
            return parse_postfix(std::make_unique<EnumValueExpr>(first.location, first.lexeme, second.lexeme));
        }
        if (match(TokenKind::LParen)) {
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenKind::RParen)) {
                do {
                    args.push_back(parse_expression());
                } while (match(TokenKind::Comma));
            }
            consume(TokenKind::RParen, "expected ')' after arguments");
            return parse_postfix(std::make_unique<CallExpr>(first.location, first.lexeme, std::move(args)));
        }
        return parse_postfix(std::make_unique<VariableExpr>(first.location, first.lexeme));
    }
    if (match(TokenKind::LParen)) {
        auto expr = parse_expression();
        consume(TokenKind::RParen, "expected ')' after expression");
        return parse_postfix(std::move(expr));
    }
    fail(current().location, "expected expression");
}

std::unique_ptr<Expr> Parser::parse_postfix(std::unique_ptr<Expr> expr) {
    while (true) {
        if (match(TokenKind::Dot)) {
            const Token field = consume(TokenKind::Identifier, "expected field name after '.'");
            if (match(TokenKind::LParen)) {
                std::vector<std::unique_ptr<Expr>> args;
                if (!check(TokenKind::RParen)) {
                    do {
                        args.push_back(parse_expression());
                    } while (match(TokenKind::Comma));
                }
                consume(TokenKind::RParen, "expected ')' after arguments");
                expr = std::make_unique<MethodCallExpr>(field.location, std::move(expr), field.lexeme, std::move(args));
            } else {
                expr = std::make_unique<FieldAccessExpr>(field.location, std::move(expr), field.lexeme);
            }
            continue;
        }
        if (match(TokenKind::LBracket)) {
            const SourceLocation location = previous().location;
            auto index = parse_expression();
            consume(TokenKind::RBracket, "expected ']' after index expression");
            expr = std::make_unique<IndexExpr>(location, std::move(expr), std::move(index));
            continue;
        }
        break;
    }
    return expr;
}

std::string Parser::parse_qualified_name() {
    std::string name = consume(TokenKind::Identifier, "expected name").lexeme;
    while (match(TokenKind::ColonColon)) {
        name += "::";
        name += consume(TokenKind::Identifier, "expected name after '::'").lexeme;
    }
    return name;
}

Type Parser::parse_type() {
    if (match(TokenKind::LBracket)) {
        Type element_type = parse_type();
        consume(TokenKind::Semicolon, "expected ';' in array type");
        const Token size_token = consume(TokenKind::Integer, "expected array size");
        consume(TokenKind::RBracket, "expected ']' after array type");
        return Type::array_type(element_type, static_cast<std::size_t>(std::stoull(size_token.lexeme)));
    }
    const Token token = consume(TokenKind::Identifier, "expected type name");
    if (token.lexeme == "int") {
        return Type::int_type();
    }
    if (token.lexeme == "bool") {
        return Type::bool_type();
    }
    if (token.lexeme == "string") {
        return Type::string_type();
    }
    if (token.lexeme == "void") {
        return Type::void_type();
    }
    return Type::struct_type(token.lexeme);
}

}  // namespace pinggen

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "pinggen/token.hpp"

namespace pinggen {

enum class ValueType {
    Int,
    Bool,
    String,
    Void
};

struct Parameter {
    SourceLocation location;
    std::string name;
    ValueType type = ValueType::Void;
};

struct Expr {
    explicit Expr(SourceLocation loc) : location(loc) {}
    virtual ~Expr() = default;
    SourceLocation location;
};

struct IntegerExpr final : Expr {
    IntegerExpr(SourceLocation loc, std::int64_t v) : Expr(loc), value(v) {}
    std::int64_t value;
};

struct StringExpr final : Expr {
    StringExpr(SourceLocation loc, std::string v) : Expr(loc), value(std::move(v)) {}
    std::string value;
};

struct BoolExpr final : Expr {
    BoolExpr(SourceLocation loc, bool v) : Expr(loc), value(v) {}
    bool value;
};

struct VariableExpr final : Expr {
    VariableExpr(SourceLocation loc, std::string n) : Expr(loc), name(std::move(n)) {}
    std::string name;
};

struct CallExpr final : Expr {
    CallExpr(SourceLocation loc, std::string c, std::vector<std::unique_ptr<Expr>> a)
        : Expr(loc), callee(std::move(c)), args(std::move(a)) {}
    std::string callee;
    std::vector<std::unique_ptr<Expr>> args;
};

struct BinaryExpr final : Expr {
    BinaryExpr(SourceLocation loc, std::string o, std::unique_ptr<Expr> l, std::unique_ptr<Expr> r)
        : Expr(loc), op(std::move(o)), left(std::move(l)), right(std::move(r)) {}
    std::string op;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct Stmt {
    explicit Stmt(SourceLocation loc) : location(loc) {}
    virtual ~Stmt() = default;
    SourceLocation location;
};

struct LetStmt final : Stmt {
    LetStmt(SourceLocation loc, std::string n, bool m, std::unique_ptr<Expr> i)
        : Stmt(loc), name(std::move(n)), is_mutable(m), initializer(std::move(i)) {}
    std::string name;
    bool is_mutable;
    std::unique_ptr<Expr> initializer;
};

struct AssignStmt final : Stmt {
    AssignStmt(SourceLocation loc, std::string n, std::unique_ptr<Expr> v)
        : Stmt(loc), name(std::move(n)), value(std::move(v)) {}
    std::string name;
    std::unique_ptr<Expr> value;
};

struct ExprStmt final : Stmt {
    ExprStmt(SourceLocation loc, std::unique_ptr<Expr> e) : Stmt(loc), expr(std::move(e)) {}
    std::unique_ptr<Expr> expr;
};

struct ReturnStmt final : Stmt {
    ReturnStmt(SourceLocation loc, std::unique_ptr<Expr> v) : Stmt(loc), value(std::move(v)) {}
    std::unique_ptr<Expr> value;
};

struct IfStmt final : Stmt {
    IfStmt(SourceLocation loc, std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Stmt>> t,
           std::vector<std::unique_ptr<Stmt>> e)
        : Stmt(loc), condition(std::move(c)), then_body(std::move(t)), else_body(std::move(e)) {}
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> then_body;
    std::vector<std::unique_ptr<Stmt>> else_body;
};

struct WhileStmt final : Stmt {
    WhileStmt(SourceLocation loc, std::unique_ptr<Expr> c, std::vector<std::unique_ptr<Stmt>> b)
        : Stmt(loc), condition(std::move(c)), body(std::move(b)) {}
    std::unique_ptr<Expr> condition;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct ImportDecl {
    SourceLocation location;
    std::vector<std::string> items;
};

struct FunctionDecl {
    SourceLocation location;
    std::string name;
    std::vector<Parameter> params;
    ValueType return_type = ValueType::Void;
    std::vector<std::unique_ptr<Stmt>> body;
};

struct Program {
    std::vector<ImportDecl> imports;
    std::vector<FunctionDecl> functions;
};

}  // namespace pinggen

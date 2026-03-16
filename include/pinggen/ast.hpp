#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "pinggen/token.hpp"

namespace pinggen {

enum class TypeKind {
    Int,
    Bool,
    String,
    Void,
    Struct
};

struct Type {
    TypeKind kind = TypeKind::Void;
    std::string name;

    static Type int_type() { return {TypeKind::Int, ""}; }
    static Type bool_type() { return {TypeKind::Bool, ""}; }
    static Type string_type() { return {TypeKind::String, ""}; }
    static Type void_type() { return {TypeKind::Void, ""}; }
    static Type struct_type(std::string struct_name) { return {TypeKind::Struct, std::move(struct_name)}; }

    bool operator==(const Type& other) const { return kind == other.kind && name == other.name; }
    bool operator!=(const Type& other) const { return !(*this == other); }
};

struct StructField {
    SourceLocation location;
    std::string name;
    Type type = Type::void_type();
};

struct Parameter {
    SourceLocation location;
    std::string name;
    Type type = Type::void_type();
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

struct StructLiteralField {
    SourceLocation location;
    std::string name;
    std::unique_ptr<Expr> value;
};

struct VariableExpr final : Expr {
    VariableExpr(SourceLocation loc, std::string n) : Expr(loc), name(std::move(n)) {}
    std::string name;
};

struct StructLiteralExpr final : Expr {
    StructLiteralExpr(SourceLocation loc, std::string n, std::vector<StructLiteralField> f)
        : Expr(loc), struct_name(std::move(n)), fields(std::move(f)) {}
    std::string struct_name;
    std::vector<StructLiteralField> fields;
};

struct FieldAccessExpr final : Expr {
    FieldAccessExpr(SourceLocation loc, std::unique_ptr<Expr> o, std::string f)
        : Expr(loc), object(std::move(o)), field(std::move(f)) {}
    std::unique_ptr<Expr> object;
    std::string field;
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

struct FieldAssignStmt final : Stmt {
    FieldAssignStmt(SourceLocation loc, std::unique_ptr<Expr> o, std::string f, std::unique_ptr<Expr> v)
        : Stmt(loc), object(std::move(o)), field(std::move(f)), value(std::move(v)) {}
    std::unique_ptr<Expr> object;
    std::string field;
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

struct BreakStmt final : Stmt {
    explicit BreakStmt(SourceLocation loc) : Stmt(loc) {}
};

struct ContinueStmt final : Stmt {
    explicit ContinueStmt(SourceLocation loc) : Stmt(loc) {}
};

struct ImportDecl {
    SourceLocation location;
    std::vector<std::string> items;
};

struct StructDecl {
    SourceLocation location;
    std::string name;
    std::vector<StructField> fields;
};

struct FunctionDecl {
    SourceLocation location;
    std::string name;
    std::vector<Parameter> params;
    Type return_type = Type::void_type();
    std::vector<std::unique_ptr<Stmt>> body;
};

struct Program {
    std::vector<ImportDecl> imports;
    std::vector<StructDecl> structs;
    std::vector<FunctionDecl> functions;
};

}  // namespace pinggen

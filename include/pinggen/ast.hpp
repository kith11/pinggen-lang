#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "pinggen/token.hpp"

namespace pinggen {

enum class TypeKind {
    Int,
    Bool,
    String,
    Void,
    Enum,
    Struct,
    Array
};

struct Type {
    TypeKind kind = TypeKind::Void;
    std::string name;
    std::shared_ptr<Type> element_type;
    std::size_t array_size = 0;

    static Type int_type() { return {TypeKind::Int, "", nullptr, 0}; }
    static Type bool_type() { return {TypeKind::Bool, "", nullptr, 0}; }
    static Type string_type() { return {TypeKind::String, "", nullptr, 0}; }
    static Type void_type() { return {TypeKind::Void, "", nullptr, 0}; }
    static Type enum_type(std::string enum_name) { return {TypeKind::Enum, std::move(enum_name), nullptr, 0}; }
    static Type struct_type(std::string struct_name) { return {TypeKind::Struct, std::move(struct_name), nullptr, 0}; }
    static Type array_type(Type element, std::size_t size) {
        return {TypeKind::Array, "", std::make_shared<Type>(std::move(element)), size};
    }

    bool operator==(const Type& other) const {
        if (kind != other.kind) {
            return false;
        }
        if (kind == TypeKind::Enum || kind == TypeKind::Struct) {
            return name == other.name;
        }
        if (kind == TypeKind::Array) {
            if (array_size != other.array_size || !element_type || !other.element_type) {
                return array_size == other.array_size && !element_type && !other.element_type;
            }
            return *element_type == *other.element_type;
        }
        return true;
    }
    bool operator!=(const Type& other) const { return !(*this == other); }
};

struct StructField {
    SourceLocation location;
    std::string name;
    Type type = Type::void_type();
};

struct EnumVariant {
    SourceLocation location;
    std::string name;
};

struct Parameter {
    SourceLocation location;
    std::string name;
    Type type = Type::void_type();
    bool is_self = false;
    bool is_mut_self = false;
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

struct ArrayLiteralExpr final : Expr {
    ArrayLiteralExpr(SourceLocation loc, std::vector<std::unique_ptr<Expr>> e) : Expr(loc), elements(std::move(e)) {}
    std::vector<std::unique_ptr<Expr>> elements;
};

struct VariableExpr final : Expr {
    VariableExpr(SourceLocation loc, std::string n) : Expr(loc), name(std::move(n)) {}
    std::string name;
};

struct EnumValueExpr final : Expr {
    EnumValueExpr(SourceLocation loc, std::string e, std::string v)
        : Expr(loc), enum_name(std::move(e)), variant(std::move(v)) {}
    std::string enum_name;
    std::string variant;
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

struct MethodCallExpr final : Expr {
    MethodCallExpr(SourceLocation loc, std::unique_ptr<Expr> o, std::string m, std::vector<std::unique_ptr<Expr>> a)
        : Expr(loc), object(std::move(o)), method(std::move(m)), args(std::move(a)) {}
    std::unique_ptr<Expr> object;
    std::string method;
    std::vector<std::unique_ptr<Expr>> args;
};

struct IndexExpr final : Expr {
    IndexExpr(SourceLocation loc, std::unique_ptr<Expr> o, std::unique_ptr<Expr> i)
        : Expr(loc), object(std::move(o)), index(std::move(i)) {}
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
};

struct UnaryExpr final : Expr {
    UnaryExpr(SourceLocation loc, std::string o, std::unique_ptr<Expr> e)
        : Expr(loc), op(std::move(o)), expr(std::move(e)) {}
    std::string op;
    std::unique_ptr<Expr> expr;
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
    LetStmt(SourceLocation loc, std::string n, bool m, std::optional<Type> t, std::unique_ptr<Expr> i)
        : Stmt(loc), name(std::move(n)), is_mutable(m), declared_type(std::move(t)), initializer(std::move(i)) {}
    std::string name;
    bool is_mutable;
    std::optional<Type> declared_type;
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

struct IndexAssignStmt final : Stmt {
    IndexAssignStmt(SourceLocation loc, std::unique_ptr<Expr> o, std::unique_ptr<Expr> i, std::unique_ptr<Expr> v)
        : Stmt(loc), object(std::move(o)), index(std::move(i)), value(std::move(v)) {}
    std::unique_ptr<Expr> object;
    std::unique_ptr<Expr> index;
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

struct ForStmt final : Stmt {
    ForStmt(SourceLocation loc, std::string n, std::unique_ptr<Expr> s, std::unique_ptr<Expr> e,
            std::vector<std::unique_ptr<Stmt>> b)
        : Stmt(loc), name(std::move(n)), start(std::move(s)), end(std::move(e)), body(std::move(b)) {}
    std::string name;
    std::unique_ptr<Expr> start;
    std::unique_ptr<Expr> end;
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

struct EnumDecl {
    SourceLocation location;
    std::string name;
    std::vector<EnumVariant> variants;
};

struct FunctionDecl {
    SourceLocation location;
    std::string name;
    std::vector<Parameter> params;
    Type return_type = Type::void_type();
    std::vector<std::unique_ptr<Stmt>> body;
    std::string impl_target;

    bool is_method() const { return !impl_target.empty(); }
    bool is_mutating_method() const { return is_method() && !params.empty() && params[0].is_mut_self; }
};

struct Program {
    std::vector<ImportDecl> imports;
    std::vector<EnumDecl> enums;
    std::vector<StructDecl> structs;
    std::vector<FunctionDecl> functions;
};

}  // namespace pinggen

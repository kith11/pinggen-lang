#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "pinggen/ast.hpp"

namespace pinggen {

struct Symbol {
    Type type = Type::void_type();
    bool is_mutable = false;
};

struct StructInfo {
    std::vector<StructField> fields;
    std::unordered_map<std::string, std::size_t> field_indices;
};

struct FunctionSignature {
    std::vector<Type> params;
    Type return_type = Type::void_type();
};

class SemanticAnalyzer {
  public:
    void analyze(const Program& program);

  private:
    void collect_structs(const Program& program);
    void collect_signatures(const Program& program);
    Type analyze_expr(const Expr& expr);
    bool analyze_block(const std::vector<std::unique_ptr<Stmt>>& body);
    bool analyze_stmt(const Stmt& stmt);
    const StructInfo& require_struct(const Type& type, const SourceLocation& location) const;
    void validate_type(const Type& type, const SourceLocation& location, bool allow_struct);

    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    Type current_return_type_ = Type::void_type();
    bool inside_main_ = false;
};

std::string type_name(const Type& type);

}  // namespace pinggen

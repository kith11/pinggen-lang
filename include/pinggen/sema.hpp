#pragma once

#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

#include "pinggen/ast.hpp"

namespace pinggen {

struct Symbol {
    Type type = Type::void_type();
    bool is_mutable = false;
    bool is_local = false;
};

struct StructInfo {
    std::vector<StructField> fields;
    std::unordered_map<std::string, std::size_t> field_indices;
};

struct EnumInfo {
    std::vector<std::optional<Type>> variant_payload_types;
    std::unordered_map<std::string, std::size_t> variant_indices;
    bool has_payload = false;
};

struct FunctionSignature {
    std::string lowered_name;
    std::vector<Type> params;
    Type return_type = Type::void_type();
    bool is_mutating_receiver = false;
};

class SemanticAnalyzer {
  public:
    void analyze(const Program& program);

  private:
    void collect_imports(const Program& program);
    void collect_enums(const Program& program);
    void collect_structs(const Program& program);
    void collect_signatures(const Program& program);
    static std::string lowered_function_name(const FunctionDecl& function);
    const FunctionSignature& require_method_signature(const Type& object_type, const std::string& method,
                                                      const SourceLocation& location) const;
    Type normalize_type(const Type& type) const;
    Type analyze_expr(const Expr& expr);
    bool analyze_block(const std::vector<std::unique_ptr<Stmt>>& body);
    bool analyze_stmt(const Stmt& stmt);
    const StructInfo& require_struct(const Type& type, const SourceLocation& location) const;
    const Type& require_array(const Type& type, const SourceLocation& location) const;
    void validate_type(const Type& type, const SourceLocation& location, bool allow_struct);
    void require_std_import(const std::string& item, const SourceLocation& location, const std::string& feature) const;

    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_set<std::string> imported_std_items_;
    std::unordered_map<std::string, EnumInfo> enums_;
    std::unordered_map<std::string, StructInfo> structs_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    std::unordered_map<std::string, std::unordered_map<std::string, FunctionSignature>> methods_;
    Type current_return_type_ = Type::void_type();
    bool inside_main_ = false;
    std::string current_method_struct_;
    std::size_t loop_depth_ = 0;
};

std::string type_name(const Type& type);

}  // namespace pinggen

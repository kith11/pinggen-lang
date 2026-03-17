#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "pinggen/ast.hpp"

namespace pinggen {

struct TypedIRValue {
    std::string ir;
    Type type = Type::void_type();
};

struct AddressValue {
    std::string address;
    Type type = Type::void_type();
};

class LLVMIRGenerator {
  public:
    std::string generate(const Program& program);

  private:
    static std::string lowered_function_name(const FunctionDecl& function);
    Type normalize_type(const Type& type) const;
    bool enum_has_payload(const std::string& enum_name) const;
    std::optional<Type> enum_payload_type(const std::string& enum_name, const std::string& variant) const;
    std::string emit_concat_helper() const;
    std::string emit_bounds_abort_helper() const;
    std::string emit_fs_read_helper() const;
    void emit_bounds_check(const std::string& index_ir, std::size_t size);
    std::string emit_enum_tag(const TypedIRValue& enum_value);
    std::string emit_string_constant(const std::string& value);
    std::string emit_con_context_type(const std::vector<Type>& capture_types, bool has_result_slot);
    std::string emit_con_task_function(const std::string& context_type_name, const Expr& item, const std::vector<Type>& capture_types,
                                       std::size_t result_index, bool has_result_slot);
    TypedIRValue emit_con_expr(const ConExpr& expr);
    TypedIRValue emit_expr(const Expr& expr);
    AddressValue emit_address(const Expr& expr);
    bool emit_block(const std::vector<std::unique_ptr<Stmt>>& body);
    bool emit_stmt(const Stmt& stmt);
    std::string llvm_type(const Type& type) const;
    std::string next_label(const std::string& prefix);
    void reset_function_state();
    std::string next_register();
    std::string next_string_name();
    static std::string escape_bytes(const std::string& value);

    std::string globals_;
    std::string functions_;
    std::string body_;
    std::unordered_map<std::string, EnumDecl> enums_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> enum_variant_indices_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> enum_payload_field_indices_;
    std::unordered_map<std::string, StructDecl> structs_;
    std::unordered_map<std::string, std::unordered_map<std::string, std::size_t>> struct_field_indices_;
    std::unordered_map<std::string, std::string> variables_;
    std::unordered_map<std::string, Type> variable_types_;
    std::unordered_map<std::string, Type> function_return_types_;
    std::unordered_map<std::string, bool> mutating_methods_;
    std::unordered_map<std::string, bool> con_safe_functions_;
    std::string current_function_name_;
    Type current_return_type_ = Type::void_type();
    std::vector<std::string> break_labels_;
    std::vector<std::string> continue_labels_;
    std::size_t register_counter_ = 0;
    std::size_t string_counter_ = 0;
    std::size_t label_counter_ = 0;
    std::size_t con_counter_ = 0;
    bool uses_con_runtime_ = false;
    std::string extra_type_defs_;
    std::string helper_functions_;
};

}  // namespace pinggen

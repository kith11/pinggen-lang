#pragma once

#include <string>
#include <unordered_map>

#include "pinggen/ast.hpp"

namespace pinggen {

struct TypedIRValue {
    std::string ir;
    ValueType type = ValueType::Void;
};

class LLVMIRGenerator {
  public:
    std::string generate(const Program& program);

  private:
    std::string emit_string_constant(const std::string& value);
    TypedIRValue emit_expr(const Expr& expr);
    bool emit_stmt(const Stmt& stmt);
    std::string llvm_type(ValueType type) const;
    void reset_function_state();
    std::string next_register();
    std::string next_string_name();
    static std::string escape_bytes(const std::string& value);

    std::string globals_;
    std::string functions_;
    std::string body_;
    std::unordered_map<std::string, std::string> variables_;
    std::unordered_map<std::string, ValueType> variable_types_;
    std::unordered_map<std::string, ValueType> function_return_types_;
    std::string current_function_name_;
    ValueType current_return_type_ = ValueType::Void;
    std::size_t register_counter_ = 0;
    std::size_t string_counter_ = 0;
};

}  // namespace pinggen

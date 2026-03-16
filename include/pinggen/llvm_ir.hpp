#pragma once

#include <string>
#include <unordered_map>

#include "pinggen/ast.hpp"

namespace pinggen {

class LLVMIRGenerator {
  public:
    std::string generate(const Program& program);

  private:
    std::string emit_string_constant(const std::string& value);
    std::string emit_expr(const Expr& expr);
    void emit_stmt(const Stmt& stmt);
    std::string next_register();
    std::string next_string_name();
    static std::string escape_bytes(const std::string& value);

    std::string globals_;
    std::string body_;
    std::unordered_map<std::string, std::string> variables_;
    std::unordered_map<std::string, std::string> variable_types_;
    std::size_t register_counter_ = 0;
    std::size_t string_counter_ = 0;
};

}  // namespace pinggen

#pragma once

#include <string>
#include <unordered_map>

#include "pinggen/ast.hpp"

namespace pinggen {

enum class ValueType {
    Int,
    String,
    Void
};

struct Symbol {
    ValueType type = ValueType::Void;
    bool is_mutable = false;
};

class SemanticAnalyzer {
  public:
    void analyze(const Program& program);

  private:
    ValueType analyze_expr(const Expr& expr);
    void analyze_stmt(const Stmt& stmt);

    std::unordered_map<std::string, Symbol> symbols_;
};

std::string type_name(ValueType type);

}  // namespace pinggen

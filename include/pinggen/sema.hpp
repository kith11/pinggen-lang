#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "pinggen/ast.hpp"

namespace pinggen {

struct Symbol {
    ValueType type = ValueType::Void;
    bool is_mutable = false;
};

struct FunctionSignature {
    std::vector<ValueType> params;
    ValueType return_type = ValueType::Void;
};

class SemanticAnalyzer {
  public:
    void analyze(const Program& program);

  private:
    void collect_signatures(const Program& program);
    ValueType analyze_expr(const Expr& expr);
    bool analyze_block(const std::vector<std::unique_ptr<Stmt>>& body);
    bool analyze_stmt(const Stmt& stmt);

    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<std::string, FunctionSignature> functions_;
    ValueType current_return_type_ = ValueType::Void;
    bool inside_main_ = false;
};

std::string type_name(ValueType type);

}  // namespace pinggen

#include "pinggen/sema.hpp"

#include "pinggen/diagnostics.hpp"

namespace pinggen {

std::string type_name(ValueType type) {
    switch (type) {
        case ValueType::Int: return "int";
        case ValueType::String: return "string";
        case ValueType::Void: return "void";
    }
    return "unknown";
}

void SemanticAnalyzer::analyze(const Program& program) {
    bool has_main = false;
    for (const auto& function : program.functions) {
        if (function.name == "main") {
            has_main = true;
        }
        symbols_.clear();
        for (const auto& stmt : function.body) {
            analyze_stmt(*stmt);
        }
    }
    if (!has_main) {
        fail({1, 1}, "program must define func main()");
    }
}

ValueType SemanticAnalyzer::analyze_expr(const Expr& expr) {
    if (dynamic_cast<const IntegerExpr*>(&expr)) {
        return ValueType::Int;
    }
    if (dynamic_cast<const StringExpr*>(&expr)) {
        return ValueType::String;
    }
    if (const auto* node = dynamic_cast<const VariableExpr*>(&expr)) {
        const auto it = symbols_.find(node->name);
        if (it == symbols_.end()) {
            fail(node->location, "unknown variable '" + node->name + "'");
        }
        return it->second.type;
    }
    if (const auto* node = dynamic_cast<const BinaryExpr*>(&expr)) {
        const ValueType left = analyze_expr(*node->left);
        const ValueType right = analyze_expr(*node->right);
        if (left != ValueType::Int || right != ValueType::Int) {
            fail(node->location, "binary operator '" + node->op + "' only supports int operands");
        }
        return ValueType::Int;
    }
    if (const auto* node = dynamic_cast<const CallExpr*>(&expr)) {
        if (node->callee == "io::println") {
            if (node->args.size() != 1) {
                fail(node->location, "io::println expects exactly one argument");
            }
            analyze_expr(*node->args[0]);
            return ValueType::Void;
        }
        fail(node->location, "unknown function '" + node->callee + "'");
    }
    fail(expr.location, "unsupported expression");
}

void SemanticAnalyzer::analyze_stmt(const Stmt& stmt) {
    if (const auto* node = dynamic_cast<const LetStmt*>(&stmt)) {
        const ValueType type = analyze_expr(*node->initializer);
        if (type == ValueType::Void) {
            fail(node->location, "variables cannot be initialized with void values");
        }
        symbols_[node->name] = Symbol{type, node->is_mutable};
        return;
    }
    if (const auto* node = dynamic_cast<const AssignStmt*>(&stmt)) {
        const auto it = symbols_.find(node->name);
        if (it == symbols_.end()) {
            fail(node->location, "unknown variable '" + node->name + "'");
        }
        if (!it->second.is_mutable) {
            fail(node->location, "cannot assign to immutable variable '" + node->name + "'");
        }
        const ValueType rhs = analyze_expr(*node->value);
        if (rhs != it->second.type) {
            fail(node->location, "cannot assign " + type_name(rhs) + " to " + type_name(it->second.type));
        }
        return;
    }
    if (const auto* node = dynamic_cast<const ExprStmt*>(&stmt)) {
        analyze_expr(*node->expr);
        return;
    }
    if (const auto* node = dynamic_cast<const ReturnStmt*>(&stmt)) {
        if (node->value) {
            const ValueType type = analyze_expr(*node->value);
            if (type != ValueType::Int) {
                fail(node->location, "main can only return int in this MVP");
            }
        }
        return;
    }
    fail(stmt.location, "unsupported statement");
}

}  // namespace pinggen

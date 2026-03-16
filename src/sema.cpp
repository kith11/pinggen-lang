#include "pinggen/sema.hpp"

#include <unordered_set>

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
    collect_signatures(program);

    bool has_main = false;
    for (const auto& function : program.functions) {
        if (function.name == "main") {
            has_main = true;
        }

        symbols_.clear();
        inside_main_ = function.name == "main";
        current_return_type_ = inside_main_ && function.return_type == ValueType::Void ? ValueType::Int : function.return_type;

        std::unordered_set<std::string> param_names;
        for (const auto& param : function.params) {
            if (!param_names.insert(param.name).second) {
                fail(param.location, "duplicate parameter '" + param.name + "'");
            }
            symbols_[param.name] = Symbol{param.type, false};
        }

        bool has_terminal_return = false;
        for (const auto& stmt : function.body) {
            has_terminal_return = analyze_stmt(*stmt) || has_terminal_return;
        }

        if (current_return_type_ != ValueType::Void && !has_terminal_return && !inside_main_) {
            fail(function.location, "non-void function '" + function.name + "' must return a value");
        }
    }
    if (!has_main) {
        fail({1, 1}, "program must define func main()");
    }
}

void SemanticAnalyzer::collect_signatures(const Program& program) {
    functions_.clear();
    for (const auto& function : program.functions) {
        if (functions_.contains(function.name)) {
            fail(function.location, "duplicate function '" + function.name + "'");
        }
        FunctionSignature signature;
        signature.return_type = function.return_type;
        for (const auto& param : function.params) {
            signature.params.push_back(param.type);
        }
        functions_[function.name] = std::move(signature);
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

        const auto it = functions_.find(node->callee);
        if (it == functions_.end()) {
            fail(node->location, "unknown function '" + node->callee + "'");
        }
        if (node->args.size() != it->second.params.size()) {
            fail(node->location, "function '" + node->callee + "' expects " +
                                     std::to_string(it->second.params.size()) + " argument(s)");
        }
        for (std::size_t i = 0; i < node->args.size(); ++i) {
            const ValueType arg_type = analyze_expr(*node->args[i]);
            if (arg_type != it->second.params[i]) {
                fail(node->args[i]->location,
                     "argument " + std::to_string(i + 1) + " of '" + node->callee + "' expects " +
                         type_name(it->second.params[i]) + " but got " + type_name(arg_type));
            }
        }
        return it->second.return_type;
    }
    fail(expr.location, "unsupported expression");
}

bool SemanticAnalyzer::analyze_stmt(const Stmt& stmt) {
    if (const auto* node = dynamic_cast<const LetStmt*>(&stmt)) {
        const ValueType type = analyze_expr(*node->initializer);
        if (type == ValueType::Void) {
            fail(node->location, "variables cannot be initialized with void values");
        }
        symbols_[node->name] = Symbol{type, node->is_mutable};
        return false;
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
        return false;
    }
    if (const auto* node = dynamic_cast<const ExprStmt*>(&stmt)) {
        analyze_expr(*node->expr);
        return false;
    }
    if (const auto* node = dynamic_cast<const ReturnStmt*>(&stmt)) {
        if (!node->value) {
            if (current_return_type_ != ValueType::Void && !inside_main_) {
                fail(node->location, "non-void function must return a " + type_name(current_return_type_));
            }
            return true;
        }
        const ValueType type = analyze_expr(*node->value);
        if (current_return_type_ == ValueType::Void) {
            fail(node->location, "void function cannot return a value");
        } else if (type != current_return_type_) {
            fail(node->location, "return type mismatch: expected " + type_name(current_return_type_) +
                                     " but got " + type_name(type));
        }
        return true;
    }
    fail(stmt.location, "unsupported statement");
}

}  // namespace pinggen

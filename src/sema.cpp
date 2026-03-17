#include "pinggen/sema.hpp"

#include <unordered_set>

#include "pinggen/diagnostics.hpp"

namespace pinggen {

namespace {

const VariableExpr* root_variable_expr(const Expr& expr) {
    if (const auto* variable = dynamic_cast<const VariableExpr*>(&expr)) {
        return variable;
    }
    if (const auto* field = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        return root_variable_expr(*field->object);
    }
    if (const auto* index = dynamic_cast<const IndexExpr*>(&expr)) {
        return root_variable_expr(*index->object);
    }
    return nullptr;
}

}  // namespace

std::string type_name(const Type& type) {
    switch (type.kind) {
        case TypeKind::Int: return "int";
        case TypeKind::Bool: return "bool";
        case TypeKind::String: return "string";
        case TypeKind::Void: return "void";
        case TypeKind::Enum: return type.name;
        case TypeKind::Struct: return type.name;
        case TypeKind::Array: return "[" + type_name(*type.element_type) + "; " + std::to_string(type.array_size) + "]";
    }
    return "unknown";
}

void SemanticAnalyzer::analyze(const Program& program) {
    collect_enums(program);
    collect_structs(program);
    collect_signatures(program);

    bool has_main = false;
    for (const auto& function : program.functions) {
        if (function.name == "main") {
            has_main = true;
        }

        symbols_.clear();
        inside_main_ = function.name == "main";
        current_method_struct_ = function.impl_target;
        const Type normalized_return_type = normalize_type(function.return_type);
        current_return_type_ = inside_main_ && normalized_return_type == Type::void_type() ? Type::int_type() : normalized_return_type;

        std::unordered_set<std::string> param_names;
        for (const auto& param : function.params) {
            const Type normalized_param_type = normalize_type(param.type);
            validate_type(normalized_param_type, param.location, true);
            if (!function.is_method() && param.name == "self") {
                fail(param.location, "'self' can only be used inside a method");
            }
            if (!param_names.insert(param.name).second) {
                fail(param.location, "duplicate parameter '" + param.name + "'");
            }
            symbols_[param.name] = Symbol{normalized_param_type, param.is_mut_self, false};
        }

        const bool has_terminal_return = analyze_block(function.body);
        if (current_return_type_ != Type::void_type() && !has_terminal_return && !inside_main_) {
            fail(function.location, "non-void function '" + function.name + "' must return a value");
        }
    }

    if (!has_main) {
        fail({1, 1}, "program must define func main()");
    }
}

void SemanticAnalyzer::collect_enums(const Program& program) {
    enums_.clear();
    for (const auto& decl : program.enums) {
        if (enums_.contains(decl.name)) {
            fail(decl.location, "duplicate enum '" + decl.name + "'");
        }
        EnumInfo info;
        std::unordered_set<std::string> seen_variants;
        for (std::size_t i = 0; i < decl.variants.size(); ++i) {
            const auto& variant = decl.variants[i];
            if (!seen_variants.insert(variant.name).second) {
                fail(variant.location, "duplicate variant '" + variant.name + "' in enum '" + decl.name + "'");
            }
            info.variant_indices[variant.name] = i;
        }
        enums_[decl.name] = std::move(info);
    }
}

void SemanticAnalyzer::collect_structs(const Program& program) {
    structs_.clear();
    for (const auto& decl : program.structs) {
        if (structs_.contains(decl.name) || enums_.contains(decl.name)) {
            fail(decl.location, "duplicate struct '" + decl.name + "'");
        }
        StructInfo info;
        std::unordered_set<std::string> seen_fields;
        for (std::size_t i = 0; i < decl.fields.size(); ++i) {
            auto field = decl.fields[i];
            field.type = normalize_type(field.type);
            if (!seen_fields.insert(field.name).second) {
                fail(field.location, "duplicate field '" + field.name + "' in struct '" + decl.name + "'");
            }
            validate_type(field.type, field.location, false);
            info.field_indices[field.name] = i;
            info.fields.push_back(field);
        }
        structs_[decl.name] = std::move(info);
    }
}

void SemanticAnalyzer::collect_signatures(const Program& program) {
    functions_.clear();
    methods_.clear();
    for (const auto& function : program.functions) {
        const Type normalized_return_type = normalize_type(function.return_type);
        validate_type(normalized_return_type, function.location, true);
        FunctionSignature signature;
        signature.lowered_name = lowered_function_name(function);
        signature.return_type = normalized_return_type;
        signature.is_mutating_receiver = function.is_mutating_method();
        if (function.is_method() && function.params.empty()) {
            fail(function.location, "methods must declare 'self' as the first parameter");
        }
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            const auto& param = function.params[i];
            if (function.is_method() && i == 0) {
                if (!param.is_self || param.name != "self") {
                    fail(param.location, "methods must declare 'self' as the first parameter");
                }
            } else if (param.is_self || param.is_mut_self || param.name == "self") {
                fail(param.location, "'self' can only be used as the first parameter of a method");
            }
            const Type normalized_param_type = normalize_type(param.type);
            validate_type(normalized_param_type, param.location, true);
            signature.params.push_back(normalized_param_type);
        }
        if (function.is_method()) {
            if (enums_.contains(function.impl_target)) {
                fail(function.location, "impl blocks only support structs in this milestone");
            }
            if (!structs_.contains(function.impl_target)) {
                fail(function.location, "unknown impl target '" + function.impl_target + "'");
            }
            auto& method_table = methods_[function.impl_target];
            if (method_table.contains(function.name)) {
                fail(function.location, "duplicate method '" + function.name + "' on struct '" + function.impl_target + "'");
            }
            method_table[function.name] = std::move(signature);
        } else {
            if (functions_.contains(function.name)) {
                fail(function.location, "duplicate function '" + function.name + "'");
            }
            functions_[function.name] = std::move(signature);
        }
    }
}

std::string SemanticAnalyzer::lowered_function_name(const FunctionDecl& function) {
    if (!function.is_method()) {
        return function.name;
    }
    return function.impl_target + "__" + function.name;
}

const FunctionSignature& SemanticAnalyzer::require_method_signature(const Type& object_type, const std::string& method,
                                                                    const SourceLocation& location) const {
    if (object_type.kind != TypeKind::Struct) {
        fail(location, "methods can only be called on struct values");
    }
    const auto methods_it = methods_.find(object_type.name);
    if (methods_it == methods_.end()) {
        fail(location, "struct '" + object_type.name + "' has no methods");
    }
    const auto method_it = methods_it->second.find(method);
    if (method_it == methods_it->second.end()) {
        fail(location, "unknown method '" + method + "' on struct '" + object_type.name + "'");
    }
    return method_it->second;
}

Type SemanticAnalyzer::normalize_type(const Type& type) const {
    if (type.kind == TypeKind::Array) {
        if (!type.element_type) {
            return type;
        }
        return Type::array_type(normalize_type(*type.element_type), type.array_size);
    }
    if (type.kind == TypeKind::Struct && enums_.contains(type.name)) {
        return Type::enum_type(type.name);
    }
    return type;
}

const StructInfo& SemanticAnalyzer::require_struct(const Type& type, const SourceLocation& location) const {
    if (type.kind != TypeKind::Struct) {
        fail(location, "expected struct value");
    }
    const auto it = structs_.find(type.name);
    if (it == structs_.end()) {
        fail(location, "unknown struct '" + type.name + "'");
    }
    return it->second;
}

const Type& SemanticAnalyzer::require_array(const Type& type, const SourceLocation& location) const {
    if (type.kind != TypeKind::Array || !type.element_type) {
        fail(location, "expected array value");
    }
    return type;
}

void SemanticAnalyzer::validate_type(const Type& type, const SourceLocation& location, bool allow_struct) {
    if (type.kind == TypeKind::Array) {
        if (!type.element_type) {
            fail(location, "invalid array element type");
        }
        validate_type(*type.element_type, location, allow_struct);
        return;
    }
    if (type.kind == TypeKind::Enum) {
        if (!enums_.contains(type.name)) {
            fail(location, "unknown enum type '" + type.name + "'");
        }
        return;
    }
    if (type.kind == TypeKind::Struct) {
        if (!allow_struct) {
            fail(location, "nested struct types are not supported in this milestone");
        }
        if (!structs_.contains(type.name)) {
            fail(location, "unknown struct type '" + type.name + "'");
        }
    }
}

Type SemanticAnalyzer::analyze_expr(const Expr& expr) {
    if (dynamic_cast<const IntegerExpr*>(&expr)) {
        return Type::int_type();
    }
    if (dynamic_cast<const BoolExpr*>(&expr)) {
        return Type::bool_type();
    }
    if (dynamic_cast<const StringExpr*>(&expr)) {
        return Type::string_type();
    }
    if (const auto* node = dynamic_cast<const EnumValueExpr*>(&expr)) {
        const auto enum_it = enums_.find(node->enum_name);
        if (enum_it == enums_.end()) {
            fail(node->location, "unknown enum '" + node->enum_name + "'");
        }
        if (!enum_it->second.variant_indices.contains(node->variant)) {
            fail(node->location, "unknown variant '" + node->variant + "' for enum '" + node->enum_name + "'");
        }
        return Type::enum_type(node->enum_name);
    }
    if (const auto* node = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
        if (node->elements.empty()) {
            fail(node->location, "empty array literals are not supported");
        }
        const Type element_type = analyze_expr(*node->elements[0]);
        if (element_type == Type::void_type()) {
            fail(node->elements[0]->location, "array elements cannot be void");
        }
        for (std::size_t i = 1; i < node->elements.size(); ++i) {
            const Type current_type = analyze_expr(*node->elements[i]);
            if (current_type != element_type) {
                fail(node->elements[i]->location,
                     "array literal element expects " + type_name(element_type) + " but got " + type_name(current_type));
            }
        }
        return Type::array_type(element_type, node->elements.size());
    }
    if (const auto* node = dynamic_cast<const VariableExpr*>(&expr)) {
        if (node->name == "self" && current_method_struct_.empty()) {
            fail(node->location, "'self' can only be used inside a method");
        }
        const auto it = symbols_.find(node->name);
        if (it == symbols_.end()) {
            fail(node->location, "unknown variable '" + node->name + "'");
        }
        return it->second.type;
    }
    if (const auto* node = dynamic_cast<const StructLiteralExpr*>(&expr)) {
        const auto struct_it = structs_.find(node->struct_name);
        if (struct_it == structs_.end()) {
            fail(node->location, "unknown struct '" + node->struct_name + "'");
        }
        const auto& info = struct_it->second;
        if (node->fields.size() != info.fields.size()) {
            fail(node->location, "struct literal for '" + node->struct_name + "' has wrong field count");
        }
        std::unordered_set<std::string> seen_fields;
        for (const auto& field : node->fields) {
            if (!seen_fields.insert(field.name).second) {
                fail(field.location, "duplicate field '" + field.name + "' in struct literal");
            }
            const auto field_it = info.field_indices.find(field.name);
            if (field_it == info.field_indices.end()) {
                fail(field.location, "unknown field '" + field.name + "' for struct '" + node->struct_name + "'");
            }
            const Type value_type = analyze_expr(*field.value);
            const Type expected = info.fields[field_it->second].type;
            if (value_type != expected) {
                fail(field.location, "field '" + field.name + "' expects " + type_name(expected) + " but got " +
                                         type_name(value_type));
            }
        }
        return Type::struct_type(node->struct_name);
    }
    if (const auto* node = dynamic_cast<const FieldAccessExpr*>(&expr)) {
        if (const auto* method_call = dynamic_cast<const MethodCallExpr*>(node->object.get())) {
            const Type method_object_type = analyze_expr(*method_call->object);
            const auto& method_signature =
                require_method_signature(method_object_type, method_call->method, method_call->location);
            if (method_signature.is_mutating_receiver) {
                fail(node->location, "mutating method calls cannot be chained");
            }
        }
        const Type object_type = analyze_expr(*node->object);
        const StructInfo& info = require_struct(object_type, node->location);
        const auto field_it = info.field_indices.find(node->field);
        if (field_it == info.field_indices.end()) {
            fail(node->location, "unknown field '" + node->field + "' on struct '" + object_type.name + "'");
        }
        return info.fields[field_it->second].type;
    }
    if (const auto* node = dynamic_cast<const IndexExpr*>(&expr)) {
        if (const auto* method_call = dynamic_cast<const MethodCallExpr*>(node->object.get())) {
            const Type method_object_type = analyze_expr(*method_call->object);
            const auto& method_signature =
                require_method_signature(method_object_type, method_call->method, method_call->location);
            if (method_signature.is_mutating_receiver) {
                fail(node->location, "mutating method calls cannot be chained");
            }
        }
        const Type object_type = analyze_expr(*node->object);
        const Type& array_type = require_array(object_type, node->location);
        const Type index_type = analyze_expr(*node->index);
        if (index_type != Type::int_type()) {
            fail(node->index->location, "array index must be int");
        }
        return *array_type.element_type;
    }
    if (const auto* node = dynamic_cast<const MethodCallExpr*>(&expr)) {
        if (const auto* prior_call = dynamic_cast<const MethodCallExpr*>(node->object.get())) {
            const Type prior_object_type = analyze_expr(*prior_call->object);
            const auto& prior_signature =
                require_method_signature(prior_object_type, prior_call->method, prior_call->location);
            if (prior_signature.is_mutating_receiver) {
                fail(node->location, "mutating method calls cannot be chained");
            }
        }
        const Type object_type = analyze_expr(*node->object);
        const auto& signature = require_method_signature(object_type, node->method, node->location);
        if (signature.is_mutating_receiver) {
            const auto* receiver_var = dynamic_cast<const VariableExpr*>(node->object.get());
            if (!receiver_var) {
                fail(node->location, "mutating methods can only be called on mutable local struct variables");
            }
            const auto symbol_it = symbols_.find(receiver_var->name);
            if (symbol_it == symbols_.end() || !symbol_it->second.is_mutable || !symbol_it->second.is_local) {
                fail(node->location, "mutating methods can only be called on mutable local struct variables");
            }
        }
        if (node->args.size() + 1 != signature.params.size()) {
            fail(node->location, "method '" + node->method + "' expects " +
                                     std::to_string(signature.params.size() - 1) + " argument(s)");
        }
        for (std::size_t i = 0; i < node->args.size(); ++i) {
            const Type arg_type = analyze_expr(*node->args[i]);
            if (arg_type != signature.params[i + 1]) {
                fail(node->args[i]->location,
                     "argument " + std::to_string(i + 1) + " of method '" + node->method + "' expects " +
                         type_name(signature.params[i + 1]) + " but got " + type_name(arg_type));
            }
        }
        return signature.return_type;
    }
    if (const auto* node = dynamic_cast<const UnaryExpr*>(&expr)) {
        const Type value = analyze_expr(*node->expr);
        if (node->op == "!") {
            if (value != Type::bool_type()) {
                fail(node->location, "unary operator '!' only supports bool operands");
            }
            return Type::bool_type();
        }
        fail(node->location, "unsupported unary operator '" + node->op + "'");
    }
    if (const auto* node = dynamic_cast<const BinaryExpr*>(&expr)) {
        const Type left = analyze_expr(*node->left);
        const Type right = analyze_expr(*node->right);
        if (node->op == "&&" || node->op == "||") {
            if (left != Type::bool_type() || right != Type::bool_type()) {
                fail(node->location, "operator '" + node->op + "' only supports bool operands");
            }
            return Type::bool_type();
        }
        if (node->op == "==" || node->op == "!=") {
            if (left != right) {
                fail(node->location, "comparison operands must have the same type");
            }
            if (left != Type::int_type() && left != Type::bool_type() && left.kind != TypeKind::Enum) {
                fail(node->location, "operator '" + node->op + "' only supports int, bool, and enum operands");
            }
            return Type::bool_type();
        }
        if (node->op == "<" || node->op == "<=" || node->op == ">" || node->op == ">=") {
            if (left != Type::int_type() || right != Type::int_type()) {
                fail(node->location, "operator '" + node->op + "' only supports int operands");
            }
            return Type::bool_type();
        }
        if (node->op == "+") {
            if (left == Type::int_type() && right == Type::int_type()) {
                return Type::int_type();
            }
            if (left == Type::string_type() && right == Type::string_type()) {
                return Type::string_type();
            }
            fail(node->location, "operator '+' only supports int+int or string+string");
        }
        if (left != Type::int_type() || right != Type::int_type()) {
            fail(node->location, "binary operator '" + node->op + "' only supports int operands");
        }
        return Type::int_type();
    }
    if (const auto* node = dynamic_cast<const CallExpr*>(&expr)) {
        if (node->callee == "io::println") {
            if (node->args.size() != 1) {
                fail(node->location, "io::println expects exactly one argument");
            }
            const Type arg_type = analyze_expr(*node->args[0]);
            if (arg_type != Type::int_type() && arg_type != Type::string_type()) {
                fail(node->args[0]->location, "io::println only supports int and string arguments");
            }
            return Type::void_type();
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
            const Type arg_type = analyze_expr(*node->args[i]);
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

bool SemanticAnalyzer::analyze_block(const std::vector<std::unique_ptr<Stmt>>& body) {
    for (const auto& stmt : body) {
        if (analyze_stmt(*stmt)) {
            return true;
        }
    }
    return false;
}

bool SemanticAnalyzer::analyze_stmt(const Stmt& stmt) {
    if (const auto* node = dynamic_cast<const LetStmt*>(&stmt)) {
        if (node->name == "self") {
            fail(node->location, "'self' is reserved for method receivers");
        }
        const Type initializer_type = analyze_expr(*node->initializer);
        if (initializer_type == Type::void_type()) {
            fail(node->location, "variables cannot be initialized with void values");
        }
        Type type = initializer_type;
        if (node->declared_type.has_value()) {
            const Type declared_type = normalize_type(*node->declared_type);
            validate_type(declared_type, node->location, true);
            if (declared_type != initializer_type) {
                fail(node->location, "cannot initialize " + type_name(declared_type) + " with " +
                                         type_name(initializer_type));
            }
            type = declared_type;
        }
        symbols_[node->name] = Symbol{type, node->is_mutable, true};
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
        const Type rhs = analyze_expr(*node->value);
        if (rhs != it->second.type) {
            fail(node->location, "cannot assign " + type_name(rhs) + " to " + type_name(it->second.type));
        }
        return false;
    }
    if (const auto* node = dynamic_cast<const FieldAssignStmt*>(&stmt)) {
        const auto* object_var = root_variable_expr(*node->object);
        if (!object_var) {
            fail(node->location, "field assignment requires a mutable variable owner");
        }
        const auto it = symbols_.find(object_var->name);
        if (it == symbols_.end()) {
            fail(node->location, "unknown variable '" + object_var->name + "'");
        }
        if (!it->second.is_mutable) {
            fail(node->location, "cannot assign through immutable variable '" + object_var->name + "'");
        }
        const Type object_type = analyze_expr(*node->object);
        const StructInfo& info = require_struct(object_type, node->location);
        const auto field_it = info.field_indices.find(node->field);
        if (field_it == info.field_indices.end()) {
            fail(node->location, "unknown field '" + node->field + "' on struct '" + object_type.name + "'");
        }
        const Type rhs = analyze_expr(*node->value);
        const Type expected = info.fields[field_it->second].type;
        if (rhs != expected) {
            fail(node->location, "cannot assign " + type_name(rhs) + " to field '" + node->field + "' of type " +
                                     type_name(expected));
        }
        return false;
    }
    if (const auto* node = dynamic_cast<const IndexAssignStmt*>(&stmt)) {
        const auto* object_var = root_variable_expr(*node->object);
        if (!object_var) {
            fail(node->location, "index assignment requires a mutable variable owner");
        }
        const auto it = symbols_.find(object_var->name);
        if (it == symbols_.end()) {
            fail(node->location, "unknown variable '" + object_var->name + "'");
        }
        if (!it->second.is_mutable) {
            fail(node->location, "cannot assign through immutable variable '" + object_var->name + "'");
        }
        const Type object_type = analyze_expr(*node->object);
        const Type& array_type = require_array(object_type, node->location);
        const Type index_type = analyze_expr(*node->index);
        if (index_type != Type::int_type()) {
            fail(node->index->location, "array index must be int");
        }
        const Type rhs = analyze_expr(*node->value);
        if (rhs != *array_type.element_type) {
            fail(node->location,
                 "cannot assign " + type_name(rhs) + " to array element of type " + type_name(*array_type.element_type));
        }
        return false;
    }
    if (const auto* node = dynamic_cast<const ExprStmt*>(&stmt)) {
        analyze_expr(*node->expr);
        return false;
    }
    if (const auto* node = dynamic_cast<const IfStmt*>(&stmt)) {
        const Type condition_type = analyze_expr(*node->condition);
        if (condition_type != Type::bool_type()) {
            fail(node->condition->location, "if condition must be bool");
        }
        const bool then_returns = analyze_block(node->then_body);
        const bool else_returns = !node->else_body.empty() && analyze_block(node->else_body);
        return then_returns && else_returns;
    }
    if (const auto* node = dynamic_cast<const MatchStmt*>(&stmt)) {
        const Type subject_type = analyze_expr(*node->subject);
        if (subject_type.kind != TypeKind::Enum) {
            fail(node->subject->location, "match subject must be an enum value");
        }
        const auto enum_it = enums_.find(subject_type.name);
        if (enum_it == enums_.end()) {
            fail(node->subject->location, "unknown enum '" + subject_type.name + "'");
        }
        std::unordered_set<std::string> seen_variants;
        bool all_return = true;
        if (node->arms.empty()) {
            fail(node->location, "match must have at least one arm");
        }
        for (const auto& arm : node->arms) {
            if (arm.enum_name != subject_type.name) {
                fail(arm.location, "match arm must use variants of enum '" + subject_type.name + "'");
            }
            if (!enum_it->second.variant_indices.contains(arm.variant)) {
                fail(arm.location, "unknown variant '" + arm.variant + "' for enum '" + arm.enum_name + "'");
            }
            if (!seen_variants.insert(arm.variant).second) {
                fail(arm.location, "duplicate match arm for variant '" + arm.variant + "'");
            }
            if (!analyze_block(arm.body)) {
                all_return = false;
            }
        }
        if (seen_variants.size() != enum_it->second.variant_indices.size()) {
            fail(node->location, "match must cover all variants of enum '" + subject_type.name + "'");
        }
        return all_return;
    }
    if (const auto* node = dynamic_cast<const WhileStmt*>(&stmt)) {
        const Type condition_type = analyze_expr(*node->condition);
        if (condition_type != Type::bool_type()) {
            fail(node->condition->location, "while condition must be bool");
        }
        ++loop_depth_;
        analyze_block(node->body);
        --loop_depth_;
        return false;
    }
    if (const auto* node = dynamic_cast<const ForStmt*>(&stmt)) {
        if (node->name == "self") {
            fail(node->location, "'self' is reserved for method receivers");
        }
        const Type start_type = analyze_expr(*node->start);
        const Type end_type = analyze_expr(*node->end);
        if (start_type != Type::int_type()) {
            fail(node->start->location, "for range start must be int");
        }
        if (end_type != Type::int_type()) {
            fail(node->end->location, "for range end must be int");
        }

        const auto previous = symbols_.find(node->name);
        const bool had_previous = previous != symbols_.end();
        Symbol previous_symbol;
        if (had_previous) {
            previous_symbol = previous->second;
        }

        symbols_[node->name] = Symbol{Type::int_type(), false, true};
        ++loop_depth_;
        analyze_block(node->body);
        --loop_depth_;

        if (had_previous) {
            symbols_[node->name] = previous_symbol;
        } else {
            symbols_.erase(node->name);
        }
        return false;
    }
    if (dynamic_cast<const BreakStmt*>(&stmt)) {
        if (loop_depth_ == 0) {
            fail(stmt.location, "'break' can only be used inside a loop");
        }
        return false;
    }
    if (dynamic_cast<const ContinueStmt*>(&stmt)) {
        if (loop_depth_ == 0) {
            fail(stmt.location, "'continue' can only be used inside a loop");
        }
        return false;
    }
    if (const auto* node = dynamic_cast<const ReturnStmt*>(&stmt)) {
        if (!node->value) {
            if (current_return_type_ != Type::void_type() && !inside_main_) {
                fail(node->location, "non-void function must return a " + type_name(current_return_type_));
            }
            return true;
        }
        const Type type = analyze_expr(*node->value);
        if (current_return_type_ == Type::void_type()) {
            fail(node->location, "void function cannot return a value");
        }
        if (type != current_return_type_) {
            fail(node->location, "return type mismatch: expected " + type_name(current_return_type_) + " but got " +
                                     type_name(type));
        }
        return true;
    }
    fail(stmt.location, "unsupported statement");
}

}  // namespace pinggen

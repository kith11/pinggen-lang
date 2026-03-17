#include "pinggen/formatter.hpp"

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "pinggen/diagnostics.hpp"
#include "pinggen/lexer.hpp"
#include "pinggen/parser.hpp"
#include "pinggen/project.hpp"

namespace pinggen {

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read source file '" + path.string() + "'");
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to write source file '" + path.string() + "'");
    }
    output << contents;
}

std::string indent(std::size_t level) { return std::string(level * 4, ' '); }

int precedence_for_expr(const Expr& expr) {
    if (dynamic_cast<const BinaryExpr*>(&expr) != nullptr) {
        const auto& binary = static_cast<const BinaryExpr&>(expr);
        if (binary.op == "||") {
            return 1;
        }
        if (binary.op == "&&") {
            return 2;
        }
        if (binary.op == "==" || binary.op == "!=") {
            return 3;
        }
        if (binary.op == "<" || binary.op == "<=" || binary.op == ">" || binary.op == ">=") {
            return 4;
        }
        if (binary.op == "+" || binary.op == "-") {
            return 5;
        }
        return 6;
    }
    if (dynamic_cast<const UnaryExpr*>(&expr) != nullptr) {
        return 7;
    }
    if (dynamic_cast<const FieldAccessExpr*>(&expr) != nullptr || dynamic_cast<const MethodCallExpr*>(&expr) != nullptr ||
        dynamic_cast<const IndexExpr*>(&expr) != nullptr || dynamic_cast<const CallExpr*>(&expr) != nullptr) {
        return 8;
    }
    return 9;
}

class Formatter {
  public:
    std::string format(const Program& program) {
        out_.str("");
        out_.clear();

        for (std::size_t i = 0; i < program.imports.size(); ++i) {
            format_import(program.imports[i]);
            out_ << '\n';
        }
        if (!program.imports.empty() &&
            (!program.enums.empty() || !program.structs.empty() || !program.functions.empty())) {
            out_ << '\n';
        }

        bool wrote_top_level = false;
        wrote_top_level = format_decls(program.enums, wrote_top_level);
        wrote_top_level = format_decls(program.structs, wrote_top_level);
        wrote_top_level = format_impls(program.functions, wrote_top_level);
        format_free_functions(program.functions, wrote_top_level);
        return out_.str();
    }

  private:
    template <typename Decl>
    bool format_decls(const std::vector<Decl>& decls, bool wrote_any) {
        for (const auto& decl : decls) {
            if (wrote_any) {
                out_ << '\n';
            }
            format_decl(decl);
            wrote_any = true;
        }
        return wrote_any;
    }

    void format_decl(const EnumDecl& decl) {
        out_ << "enum " << decl.name << " {\n";
        for (const auto& variant : decl.variants) {
            out_ << indent(1) << variant.name;
            if (variant.payload_type.has_value()) {
                out_ << '(' << format_type(*variant.payload_type) << ')';
            }
            out_ << ",\n";
        }
        out_ << "}\n";
    }

    void format_decl(const StructDecl& decl) {
        out_ << "struct " << decl.name << " {\n";
        for (const auto& field : decl.fields) {
            out_ << indent(1) << field.name << ": " << format_type(field.type) << ",\n";
        }
        out_ << "}\n";
    }

    bool format_impls(const std::vector<FunctionDecl>& functions, bool wrote_any) {
        std::vector<std::string> order;
        std::map<std::string, std::vector<const FunctionDecl*>> groups;
        for (const auto& function : functions) {
            if (!function.is_method()) {
                continue;
            }
            if (!groups.contains(function.impl_target)) {
                order.push_back(function.impl_target);
            }
            groups[function.impl_target].push_back(&function);
        }

        for (const auto& impl_target : order) {
            if (wrote_any) {
                out_ << '\n';
            }
            out_ << "impl " << impl_target << " {\n";
            const auto& group = groups[impl_target];
            for (std::size_t i = 0; i < group.size(); ++i) {
                format_function(*group[i], 1);
                if (i + 1 < group.size()) {
                    out_ << '\n';
                }
            }
            out_ << "}\n";
            wrote_any = true;
        }
        return wrote_any;
    }

    void format_free_functions(const std::vector<FunctionDecl>& functions, bool wrote_any) {
        for (const auto& function : functions) {
            if (function.is_method()) {
                continue;
            }
            if (wrote_any) {
                out_ << '\n';
            }
            format_function(function, 0);
            wrote_any = true;
        }
    }

    void format_import(const ImportDecl& decl) {
        if (decl.kind == ImportKind::Std) {
            out_ << "import std::{ ";
            for (std::size_t i = 0; i < decl.items.size(); ++i) {
                if (i > 0) {
                    out_ << ", ";
                }
                out_ << decl.items[i];
            }
            out_ << " }";
            return;
        }
        out_ << "import " << decl.module_name << ';';
    }

    void format_function(const FunctionDecl& decl, std::size_t indent_level) {
        out_ << indent(indent_level);
        if (decl.is_con_safe) {
            out_ << "safe ";
        }
        out_ << "func " << decl.name << '(';
        bool first = true;
        for (const auto& param : decl.params) {
            if (!first) {
                out_ << ", ";
            }
            first = false;
            if (param.is_self) {
                if (param.is_mut_self) {
                    out_ << "mut self";
                } else {
                    out_ << "self";
                }
            } else {
                out_ << param.name << ": " << format_type(param.type);
            }
        }
        out_ << ')';
        if (decl.return_type.kind != TypeKind::Void) {
            out_ << " -> " << format_type(decl.return_type);
        }
        out_ << " {\n";
        format_stmt_list(decl.body, indent_level + 1);
        out_ << indent(indent_level) << "}\n";
    }

    void format_stmt_list(const std::vector<std::unique_ptr<Stmt>>& statements, std::size_t indent_level) {
        for (const auto& stmt : statements) {
            format_stmt(*stmt, indent_level);
        }
    }

    void format_stmt(const Stmt& stmt, std::size_t indent_level) {
        if (const auto* let_stmt = dynamic_cast<const LetStmt*>(&stmt)) {
            out_ << indent(indent_level) << "let ";
            if (let_stmt->is_mutable) {
                out_ << "mut ";
            }
            out_ << let_stmt->name;
            if (let_stmt->declared_type.has_value()) {
                out_ << ": " << format_type(*let_stmt->declared_type);
            }
            out_ << " = " << format_expr(*let_stmt->initializer) << ";\n";
            return;
        }
        if (const auto* tuple_let = dynamic_cast<const TupleLetStmt*>(&stmt)) {
            out_ << indent(indent_level) << "let ";
            if (tuple_let->is_mutable) {
                out_ << "mut ";
            }
            out_ << '(';
            for (std::size_t i = 0; i < tuple_let->names.size(); ++i) {
                if (i > 0) {
                    out_ << ", ";
                }
                out_ << tuple_let->names[i];
            }
            out_ << ") = " << format_expr(*tuple_let->initializer) << ";\n";
            return;
        }
        if (const auto* assign_stmt = dynamic_cast<const AssignStmt*>(&stmt)) {
            out_ << indent(indent_level) << assign_stmt->name << " = " << format_expr(*assign_stmt->value) << ";\n";
            return;
        }
        if (const auto* field_assign = dynamic_cast<const FieldAssignStmt*>(&stmt)) {
            out_ << indent(indent_level) << format_expr(*field_assign->object, 8) << '.' << field_assign->field << " = "
                 << format_expr(*field_assign->value) << ";\n";
            return;
        }
        if (const auto* index_assign = dynamic_cast<const IndexAssignStmt*>(&stmt)) {
            out_ << indent(indent_level) << format_expr(*index_assign->object, 8) << '['
                 << format_expr(*index_assign->index) << "] = " << format_expr(*index_assign->value) << ";\n";
            return;
        }
        if (const auto* expr_stmt = dynamic_cast<const ExprStmt*>(&stmt)) {
            out_ << indent(indent_level) << format_expr(*expr_stmt->expr) << ";\n";
            return;
        }
        if (const auto* return_stmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
            out_ << indent(indent_level) << "return";
            if (return_stmt->value) {
                out_ << ' ' << format_expr(*return_stmt->value);
            }
            out_ << ";\n";
            return;
        }
        if (const auto* if_stmt = dynamic_cast<const IfStmt*>(&stmt)) {
            format_if_stmt(*if_stmt, indent_level);
            return;
        }
        if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(&stmt)) {
            out_ << indent(indent_level) << "while " << format_expr(*while_stmt->condition) << " {\n";
            format_stmt_list(while_stmt->body, indent_level + 1);
            out_ << indent(indent_level) << "}\n";
            return;
        }
        if (const auto* for_stmt = dynamic_cast<const ForStmt*>(&stmt)) {
            out_ << indent(indent_level) << "for " << for_stmt->name << " in " << format_expr(*for_stmt->start) << ".."
                 << format_expr(*for_stmt->end) << " {\n";
            format_stmt_list(for_stmt->body, indent_level + 1);
            out_ << indent(indent_level) << "}\n";
            return;
        }
        if (const auto* match_stmt = dynamic_cast<const MatchStmt*>(&stmt)) {
            out_ << indent(indent_level) << "match " << format_expr(*match_stmt->subject) << " {\n";
            for (const auto& arm : match_stmt->arms) {
                out_ << indent(indent_level + 1) << arm.enum_name << "::" << arm.variant;
                if (arm.binding_name.has_value()) {
                    out_ << '(' << *arm.binding_name << ')';
                }
                out_ << " => {\n";
                format_stmt_list(arm.body, indent_level + 2);
                out_ << indent(indent_level + 1) << "}\n";
            }
            out_ << indent(indent_level) << "}\n";
            return;
        }
        if (dynamic_cast<const BreakStmt*>(&stmt) != nullptr) {
            out_ << indent(indent_level) << "break;\n";
            return;
        }
        if (dynamic_cast<const ContinueStmt*>(&stmt) != nullptr) {
            out_ << indent(indent_level) << "continue;\n";
            return;
        }
        throw std::runtime_error("formatter encountered unsupported statement");
    }

    void format_if_stmt(const IfStmt& stmt, std::size_t indent_level) {
        out_ << indent(indent_level) << "if " << format_expr(*stmt.condition) << " {\n";
        format_stmt_list(stmt.then_body, indent_level + 1);
        out_ << indent(indent_level) << "}";
        if (stmt.else_body.empty()) {
            out_ << '\n';
            return;
        }
        if (stmt.else_body.size() == 1) {
            if (const auto* nested_if = dynamic_cast<const IfStmt*>(stmt.else_body[0].get())) {
                out_ << " else ";
                format_if_stmt_inline(*nested_if, indent_level);
                return;
            }
        }
        out_ << " else {\n";
        format_stmt_list(stmt.else_body, indent_level + 1);
        out_ << indent(indent_level) << "}\n";
    }

    void format_if_stmt_inline(const IfStmt& stmt, std::size_t indent_level) {
        out_ << "if " << format_expr(*stmt.condition) << " {\n";
        format_stmt_list(stmt.then_body, indent_level + 1);
        out_ << indent(indent_level) << "}";
        if (stmt.else_body.empty()) {
            out_ << '\n';
            return;
        }
        if (stmt.else_body.size() == 1) {
            if (const auto* nested_if = dynamic_cast<const IfStmt*>(stmt.else_body[0].get())) {
                out_ << " else ";
                format_if_stmt_inline(*nested_if, indent_level);
                return;
            }
        }
        out_ << " else {\n";
        format_stmt_list(stmt.else_body, indent_level + 1);
        out_ << indent(indent_level) << "}\n";
    }

    std::string format_type(const Type& type) {
        switch (type.kind) {
            case TypeKind::Int: return "int";
            case TypeKind::Bool: return "bool";
            case TypeKind::String: return "string";
            case TypeKind::Void: return "void";
            case TypeKind::Enum:
            case TypeKind::Struct: return type.name;
            case TypeKind::Array:
                return "[" + format_type(*type.element_type) + "; " + std::to_string(type.array_size) + "]";
            case TypeKind::Tuple: {
                std::string value = "(";
                for (std::size_t i = 0; i < type.tuple_elements.size(); ++i) {
                    if (i > 0) {
                        value += ", ";
                    }
                    value += format_type(type.tuple_elements[i]);
                }
                value += ")";
                return value;
            }
            case TypeKind::Vec: return "Vec<" + format_type(*type.element_type) + ">";
        }
        throw std::runtime_error("formatter encountered unsupported type");
    }

    std::string format_expr(const Expr& expr, int parent_precedence = 0) {
        const int current_precedence = precedence_for_expr(expr);
        std::string value;

        if (const auto* integer_expr = dynamic_cast<const IntegerExpr*>(&expr)) {
            value = std::to_string(integer_expr->value);
        } else if (const auto* string_expr = dynamic_cast<const StringExpr*>(&expr)) {
            value = quote_string(string_expr->value);
        } else if (const auto* bool_expr = dynamic_cast<const BoolExpr*>(&expr)) {
            value = bool_expr->value ? "true" : "false";
        } else if (const auto* variable_expr = dynamic_cast<const VariableExpr*>(&expr)) {
            value = variable_expr->name;
        } else if (const auto* tuple_expr = dynamic_cast<const TupleExpr*>(&expr)) {
            value = "(" + join_exprs(tuple_expr->elements) + ")";
        } else if (const auto* array_expr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
            value = "[" + join_exprs(array_expr->elements) + "]";
        } else if (const auto* vec_expr = dynamic_cast<const VecLiteralExpr*>(&expr)) {
            value = "vec";
            if (vec_expr->declared_element_type.has_value()) {
                value += "<" + format_type(*vec_expr->declared_element_type) + ">";
            }
            value += "[" + join_exprs(vec_expr->elements) + "]";
        } else if (const auto* enum_expr = dynamic_cast<const EnumValueExpr*>(&expr)) {
            value = enum_expr->enum_name + "::" + enum_expr->variant;
            if (enum_expr->uses_call_syntax || enum_expr->payload) {
                value += "(";
                if (enum_expr->payload) {
                    value += format_expr(*enum_expr->payload);
                }
                value += ")";
            }
        } else if (const auto* struct_expr = dynamic_cast<const StructLiteralExpr*>(&expr)) {
            if (struct_expr->fields.empty()) {
                value = struct_expr->struct_name + " {}";
            } else {
                std::ostringstream nested;
                nested << struct_expr->struct_name << " {\n";
                for (const auto& field : struct_expr->fields) {
                    nested << indent(1) << field.name << ": " << format_expr(*field.value) << ",\n";
                }
                nested << "}";
                value = nested.str();
            }
        } else if (const auto* field_expr = dynamic_cast<const FieldAccessExpr*>(&expr)) {
            value = format_expr(*field_expr->object, current_precedence) + "." + field_expr->field;
        } else if (const auto* method_expr = dynamic_cast<const MethodCallExpr*>(&expr)) {
            value = format_expr(*method_expr->object, current_precedence) + "." + method_expr->method + "(" +
                    join_exprs(method_expr->args) + ")";
        } else if (const auto* index_expr = dynamic_cast<const IndexExpr*>(&expr)) {
            value = format_expr(*index_expr->object, current_precedence) + "[" + format_expr(*index_expr->index) + "]";
        } else if (const auto* unary_expr = dynamic_cast<const UnaryExpr*>(&expr)) {
            value = unary_expr->op + format_expr(*unary_expr->expr, current_precedence);
        } else if (const auto* call_expr = dynamic_cast<const CallExpr*>(&expr)) {
            value = call_expr->callee + "(" + join_exprs(call_expr->args) + ")";
        } else if (const auto* con_expr = dynamic_cast<const ConExpr*>(&expr)) {
            value = "con { " + join_exprs(con_expr->items) + " }";
        } else if (const auto* binary_expr = dynamic_cast<const BinaryExpr*>(&expr)) {
            value = format_expr(*binary_expr->left, current_precedence) + " " + binary_expr->op + " " +
                    format_expr(*binary_expr->right, current_precedence + 1);
        } else {
            throw std::runtime_error("formatter encountered unsupported expression");
        }

        if (current_precedence < parent_precedence) {
            return "(" + value + ")";
        }
        return value;
    }

    std::string join_exprs(const std::vector<std::unique_ptr<Expr>>& expressions) {
        std::string value;
        for (std::size_t i = 0; i < expressions.size(); ++i) {
            if (i > 0) {
                value += ", ";
            }
            value += format_expr(*expressions[i]);
        }
        return value;
    }

    static std::string quote_string(const std::string& value) {
        std::string escaped = "\"";
        for (const char c : value) {
            switch (c) {
                case '\n': escaped += "\\n"; break;
                case '\t': escaped += "\\t"; break;
                case '"': escaped += "\\\""; break;
                case '\\': escaped += "\\\\"; break;
                default: escaped.push_back(c); break;
            }
        }
        escaped += '"';
        return escaped;
    }

    std::ostringstream out_;
};

void validate_comment_support(const std::string& source) {
    std::istringstream lines(source);
    std::string line;
    std::size_t line_number = 1;
    while (std::getline(lines, line)) {
        const std::size_t comment_index = line.find('#');
        if (comment_index == std::string::npos) {
            ++line_number;
            continue;
        }
        const std::string prefix = line.substr(0, comment_index);
        if (prefix.find_first_not_of(" \t\r") != std::string::npos) {
            throw std::runtime_error("formatter does not support inline trailing comments yet (line " +
                                     std::to_string(line_number) + ")");
        }
        throw std::runtime_error("formatter does not support comment lines yet (line " + std::to_string(line_number) + ")");
    }
}

std::vector<std::filesystem::path> collect_pg_files(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> files;
    const std::filesystem::path source_root = root / "src";
    if (!std::filesystem::exists(source_root)) {
        return files;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source_root)) {
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() == ".pg") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

}  // namespace

std::string format_program(const Program& program) {
    Formatter formatter;
    return formatter.format(program);
}

std::string format_source(const std::string& source) {
    validate_comment_support(source);
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    const Program program = parser.parse();
    return format_program(program);
}

FormatterSummary format_project_sources(const std::filesystem::path& project_dir, bool check_only) {
    const ProjectConfig project = load_project(project_dir);
    FormatterSummary summary;
    for (const auto& path : collect_pg_files(project.root)) {
        summary.visited_files.push_back(path);
        const std::string original = read_file(path);
        const std::string formatted = format_source(original);
        if (formatted == original) {
            continue;
        }
        summary.changed_files.push_back(path);
        if (!check_only) {
            write_file(path, formatted);
        }
    }
    return summary;
}

}  // namespace pinggen

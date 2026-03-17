#include "pinggen/lsp.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "pinggen/diagnostics.hpp"
#include "pinggen/frontend.hpp"
#include "pinggen/sema.hpp"

namespace fs = std::filesystem;

namespace pinggen {

namespace {

struct JsonValue;
using JsonObject = std::map<std::string, JsonValue>;
using JsonArray = std::vector<JsonValue>;

struct JsonValue {
    using Storage = std::variant<std::nullptr_t, bool, double, std::string, JsonArray, JsonObject>;
    Storage value = nullptr;

    JsonValue() = default;
    JsonValue(std::nullptr_t) : value(nullptr) {}
    JsonValue(bool v) : value(v) {}
    JsonValue(double v) : value(v) {}
    JsonValue(int v) : value(static_cast<double>(v)) {}
    JsonValue(std::string v) : value(std::move(v)) {}
    JsonValue(const char* v) : value(std::string(v)) {}
    JsonValue(JsonArray v) : value(std::move(v)) {}
    JsonValue(JsonObject v) : value(std::move(v)) {}

    bool is_null() const { return std::holds_alternative<std::nullptr_t>(value); }
    bool is_bool() const { return std::holds_alternative<bool>(value); }
    bool is_number() const { return std::holds_alternative<double>(value); }
    bool is_string() const { return std::holds_alternative<std::string>(value); }
    bool is_array() const { return std::holds_alternative<JsonArray>(value); }
    bool is_object() const { return std::holds_alternative<JsonObject>(value); }

    const std::string& as_string() const { return std::get<std::string>(value); }
    double as_number() const { return std::get<double>(value); }
    bool as_bool() const { return std::get<bool>(value); }
    const JsonArray& as_array() const { return std::get<JsonArray>(value); }
    const JsonObject& as_object() const { return std::get<JsonObject>(value); }
};

class JsonParser {
  public:
    explicit JsonParser(std::string text) : text_(std::move(text)) {}
    JsonValue parse();

  private:
    char peek() const;
    char advance();
    void skip_ws();
    void expect(char ch);
    JsonValue parse_value();
    JsonObject parse_object();
    JsonArray parse_array();
    std::string parse_string();
    double parse_number();
    void consume_literal(const char* literal);

    std::string text_;
    std::size_t index_ = 0;
};

struct Span {
    fs::path path;
    std::size_t line = 1;
    std::size_t column = 1;
    std::size_t length = 1;
};

struct SymbolEntry {
    Span span;
    std::optional<Span> definition;
    std::string hover;
};

struct CompletionCandidate {
    std::string label;
    int kind = 1;
    std::string detail;
    std::string insert_text;
};

struct AnalysisResult {
    std::vector<JsonObject> diagnostics;
    std::vector<fs::path> touched_files;
    std::vector<SymbolEntry> symbols;
};

std::string stringify_json(const JsonValue& value);
const JsonValue* object_get(const JsonObject& object, const std::string& key);
std::optional<std::string> object_get_string(const JsonObject& object, const std::string& key);
std::optional<int> object_get_int(const JsonObject& object, const std::string& key);
std::string path_to_uri(const fs::path& path);
fs::path uri_to_path(const std::string& uri);
JsonObject make_range(const Span& span);
JsonObject make_location(const Span& span);
bool span_contains(const Span& span, std::size_t line_zero, std::size_t character_zero);
Span span_from_location(const SourceLocation& location, std::size_t length);
std::string function_hover(const FunctionDecl& decl);
AnalysisResult analyze_project(const fs::path& project_root, const std::unordered_map<std::string, std::string>& source_overrides);
std::string read_file(const fs::path& path);

class LspServer {
  public:
    int run();

  private:
    std::optional<JsonObject> read_message();
    void send_json(const JsonObject& object);
    void send_response(const JsonValue& id, JsonValue result);
    void send_error(const JsonValue& id, int code, const std::string& message);
    void publish_diagnostics(const AnalysisResult& analysis);
    std::unordered_map<std::string, std::string> overrides_for_project(const fs::path& project_root) const;
    AnalysisResult analyze_document_project(const std::string& uri);
    const SymbolEntry* find_symbol_at(const std::vector<SymbolEntry>& symbols, const fs::path& path, std::size_t line,
                                      std::size_t character) const;
    bool handle_message(const JsonObject& message);
    void handle_document_sync(const std::string& method, const JsonObject& params);
    JsonValue handle_definition(const JsonObject& params);
    JsonValue handle_hover(const JsonObject& params);
    JsonValue handle_completion(const JsonObject& params);

    std::unordered_map<std::string, std::string> open_documents_;
    bool shutdown_requested_ = false;
    int exit_code_ = 0;
};

JsonValue JsonParser::parse() {
    skip_ws();
    JsonValue value = parse_value();
    skip_ws();
    if (index_ != text_.size()) {
        throw std::runtime_error("unexpected trailing JSON content");
    }
    return value;
}

char JsonParser::peek() const { return index_ < text_.size() ? text_[index_] : '\0'; }

char JsonParser::advance() { return index_ < text_.size() ? text_[index_++] : '\0'; }

void JsonParser::skip_ws() {
    while (std::isspace(static_cast<unsigned char>(peek()))) {
        ++index_;
    }
}

void JsonParser::expect(char ch) {
    if (advance() != ch) {
        throw std::runtime_error("invalid JSON");
    }
}

JsonValue JsonParser::parse_value() {
    switch (peek()) {
        case '{': return JsonObject(parse_object());
        case '[': return JsonArray(parse_array());
        case '"': return JsonValue(parse_string());
        case 't':
            consume_literal("true");
            return JsonValue(true);
        case 'f':
            consume_literal("false");
            return JsonValue(false);
        case 'n':
            consume_literal("null");
            return JsonValue(nullptr);
        default:
            if (peek() == '-' || std::isdigit(static_cast<unsigned char>(peek()))) {
                return JsonValue(parse_number());
            }
            throw std::runtime_error("invalid JSON value");
    }
}

JsonObject JsonParser::parse_object() {
    JsonObject object;
    expect('{');
    skip_ws();
    if (peek() == '}') {
        advance();
        return object;
    }
    while (true) {
        skip_ws();
        const std::string key = parse_string();
        skip_ws();
        expect(':');
        skip_ws();
        object.emplace(key, parse_value());
        skip_ws();
        if (peek() == '}') {
            advance();
            break;
        }
        expect(',');
        skip_ws();
    }
    return object;
}

JsonArray JsonParser::parse_array() {
    JsonArray array;
    expect('[');
    skip_ws();
    if (peek() == ']') {
        advance();
        return array;
    }
    while (true) {
        skip_ws();
        array.push_back(parse_value());
        skip_ws();
        if (peek() == ']') {
            advance();
            break;
        }
        expect(',');
        skip_ws();
    }
    return array;
}

std::string JsonParser::parse_string() {
    expect('"');
    std::string value;
    while (true) {
        const char ch = advance();
        if (ch == '\0') {
            throw std::runtime_error("unterminated JSON string");
        }
        if (ch == '"') {
            break;
        }
        if (ch == '\\') {
            const char escaped = advance();
            switch (escaped) {
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                case '/': value.push_back('/'); break;
                case 'b': value.push_back('\b'); break;
                case 'f': value.push_back('\f'); break;
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case 'u':
                    for (int i = 0; i < 4; ++i) {
                        if (!std::isxdigit(static_cast<unsigned char>(advance()))) {
                            throw std::runtime_error("invalid unicode escape");
                        }
                    }
                    value.push_back('?');
                    break;
                default: throw std::runtime_error("invalid JSON escape");
            }
            continue;
        }
        value.push_back(ch);
    }
    return value;
}

double JsonParser::parse_number() {
    const std::size_t start = index_;
    if (peek() == '-') {
        advance();
    }
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }
    if (peek() == '.') {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }
    if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+' || peek() == '-') {
            advance();
        }
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }
    return std::stod(text_.substr(start, index_ - start));
}

void JsonParser::consume_literal(const char* literal) {
    while (*literal != '\0') {
        if (advance() != *literal++) {
            throw std::runtime_error("invalid JSON literal");
        }
    }
}

std::string json_escape(const std::string& value) {
    std::string out;
    for (const char ch : value) {
        switch (ch) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out += '?';
                } else {
                    out.push_back(ch);
                }
                break;
        }
    }
    return out;
}

std::string stringify_json(const JsonValue& value) {
    if (value.is_null()) {
        return "null";
    }
    if (value.is_bool()) {
        return value.as_bool() ? "true" : "false";
    }
    if (value.is_number()) {
        std::ostringstream out;
        const double number = value.as_number();
        if (number == static_cast<double>(static_cast<long long>(number))) {
            out << static_cast<long long>(number);
        } else {
            out << number;
        }
        return out.str();
    }
    if (value.is_string()) {
        return "\"" + json_escape(value.as_string()) + "\"";
    }
    if (value.is_array()) {
        std::string out = "[";
        bool first = true;
        for (const auto& item : value.as_array()) {
            if (!first) {
                out += ",";
            }
            first = false;
            out += stringify_json(item);
        }
        out += "]";
        return out;
    }
    std::string out = "{";
    bool first = true;
    for (const auto& [key, item] : value.as_object()) {
        if (!first) {
            out += ",";
        }
        first = false;
        out += "\"" + json_escape(key) + "\":" + stringify_json(item);
    }
    out += "}";
    return out;
}

const JsonValue* object_get(const JsonObject& object, const std::string& key) {
    const auto it = object.find(key);
    return it == object.end() ? nullptr : &it->second;
}

std::optional<std::string> object_get_string(const JsonObject& object, const std::string& key) {
    const JsonValue* value = object_get(object, key);
    if (!value || !value->is_string()) {
        return std::nullopt;
    }
    return value->as_string();
}

std::optional<int> object_get_int(const JsonObject& object, const std::string& key) {
    const JsonValue* value = object_get(object, key);
    if (!value || !value->is_number()) {
        return std::nullopt;
    }
    return static_cast<int>(value->as_number());
}

std::string path_to_uri(const fs::path& path) {
    std::string normalized = fs::weakly_canonical(path).generic_string();
    if (normalized.size() >= 2 && normalized[1] == ':') {
        normalized = "/" + normalized;
    }
    return "file://" + normalized;
}

fs::path uri_to_path(const std::string& uri) {
    const std::string prefix = "file://";
    if (uri.rfind(prefix, 0) != 0) {
        throw std::runtime_error("unsupported URI: " + uri);
    }
    std::string path = uri.substr(prefix.size());
    if (path.size() >= 3 && path[0] == '/' && path[2] == ':') {
        path.erase(path.begin());
    }
    std::replace(path.begin(), path.end(), '/', static_cast<char>(fs::path::preferred_separator));
    return fs::path(path);
}

JsonObject make_range(const Span& span) {
    return {
        {"start", JsonObject{{"line", static_cast<int>(span.line - 1)}, {"character", static_cast<int>(span.column - 1)}}},
        {"end", JsonObject{{"line", static_cast<int>(span.line - 1)},
                             {"character", static_cast<int>(span.column - 1 + span.length)}}}};
}

JsonObject make_location(const Span& span) { return {{"uri", path_to_uri(span.path)}, {"range", make_range(span)}}; }

bool span_contains(const Span& span, std::size_t line_zero, std::size_t character_zero) {
    if (line_zero + 1 != span.line) {
        return false;
    }
    const std::size_t start = span.column - 1;
    const std::size_t end = start + std::max<std::size_t>(span.length, 1);
    return character_zero >= start && character_zero < end;
}

Span span_from_location(const SourceLocation& location, std::size_t length) {
    return {fs::path(location.file), location.line, location.column, std::max<std::size_t>(length, 1)};
}

std::string function_hover(const FunctionDecl& decl) {
    std::string text;
    if (decl.is_con_safe) {
        text += "safe ";
    }
    text += "func " + decl.name + "(";
    for (std::size_t i = 0; i < decl.params.size(); ++i) {
        if (i > 0) {
            text += ", ";
        }
        const auto& param = decl.params[i];
        if (param.is_self) {
            text += param.is_mut_self ? "mut self" : "self";
        } else {
            text += param.name + ": " + type_name(param.type);
        }
    }
    text += ")";
    if (decl.return_type.kind != TypeKind::Void) {
        text += " -> " + type_name(decl.return_type);
    }
    return text;
}

std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read source file '" + path.string() + "'");
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

bool is_identifier_char(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::stringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    if (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        lines.emplace_back();
    }
    if (lines.empty()) {
        lines.emplace_back();
    }
    return lines;
}

std::vector<std::string> split_module_name(const std::string& module_name) {
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start < module_name.size()) {
        const std::size_t separator = module_name.find("::", start);
        if (separator == std::string::npos) {
            segments.push_back(module_name.substr(start));
            break;
        }
        segments.push_back(module_name.substr(start, separator - start));
        start = separator + 2;
    }
    return segments;
}

std::string join_module_segments(const std::vector<std::string>& segments, std::size_t start_index = 0) {
    std::string module_name;
    for (std::size_t i = start_index; i < segments.size(); ++i) {
        if (!module_name.empty()) {
            module_name += "::";
        }
        module_name += segments[i];
    }
    return module_name;
}

enum class CompletionContextKind {
    Plain,
    Member,
    Namespace
};

struct CompletionContext {
    CompletionContextKind kind = CompletionContextKind::Plain;
    std::string prefix;
    std::string receiver;
    bool in_import = false;
    bool in_std_import = false;
};

std::string trim_left(std::string text) {
    const auto it =
        std::find_if(text.begin(), text.end(), [](char ch) { return !std::isspace(static_cast<unsigned char>(ch)); });
    text.erase(text.begin(), it);
    return text;
}

std::string scan_backward_member_receiver(const std::string& prefix) {
    std::size_t end = prefix.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(prefix[end - 1]))) {
        --end;
    }
    std::size_t start = end;
    while (start > 0 && (is_identifier_char(prefix[start - 1]) || prefix[start - 1] == '.')) {
        --start;
    }
    return prefix.substr(start, end - start);
}

std::string scan_backward_namespace_receiver(const std::string& prefix) {
    std::size_t end = prefix.size();
    while (end > 0 && std::isspace(static_cast<unsigned char>(prefix[end - 1]))) {
        --end;
    }
    std::size_t start = end;
    while (start > 0 && (is_identifier_char(prefix[start - 1]) || prefix[start - 1] == ':')) {
        --start;
    }
    return prefix.substr(start, end - start);
}

CompletionContext classify_completion_context(const std::string& text, std::size_t line_zero, std::size_t character_zero) {
    const std::vector<std::string> lines = split_lines(text);
    const std::size_t line_index = std::min<std::size_t>(line_zero, lines.size() - 1);
    const std::string& line = lines[line_index];
    const std::size_t cursor = std::min<std::size_t>(character_zero, line.size());
    const std::string line_prefix = line.substr(0, cursor);

    CompletionContext context;
    const std::string trimmed = trim_left(line_prefix);
    context.in_import = starts_with(trimmed, "import ");
    context.in_std_import = context.in_import && trimmed.find("import std::{") != std::string::npos;

    std::size_t prefix_start = cursor;
    while (prefix_start > 0 && is_identifier_char(line[prefix_start - 1])) {
        --prefix_start;
    }
    context.prefix = line.substr(prefix_start, cursor - prefix_start);

    const std::string before_prefix = line.substr(0, prefix_start);
    if (before_prefix.size() >= 2 && before_prefix.substr(before_prefix.size() - 2) == "::") {
        context.kind = CompletionContextKind::Namespace;
        context.receiver = scan_backward_namespace_receiver(before_prefix.substr(0, before_prefix.size() - 2));
        return context;
    }
    if (!before_prefix.empty() && before_prefix.back() == '.') {
        context.kind = CompletionContextKind::Member;
        context.receiver = scan_backward_member_receiver(before_prefix.substr(0, before_prefix.size() - 1));
        return context;
    }
    if (cursor >= 2 && line.substr(cursor - 2, 2) == "::") {
        context.kind = CompletionContextKind::Namespace;
        context.prefix.clear();
        context.receiver = scan_backward_namespace_receiver(line.substr(0, cursor - 2));
        return context;
    }
    if (cursor >= 1 && line[cursor - 1] == '.') {
        context.kind = CompletionContextKind::Member;
        context.prefix.clear();
        context.receiver = scan_backward_member_receiver(line.substr(0, cursor - 1));
        return context;
    }
    return context;
}

bool location_before_or_equal(const SourceLocation& location, const fs::path& file, std::size_t line_zero, std::size_t character_zero) {
    if (fs::weakly_canonical(location.file).string() != fs::weakly_canonical(file).string()) {
        return false;
    }
    const std::size_t line = line_zero + 1;
    const std::size_t column = character_zero + 1;
    if (location.line < line) {
        return true;
    }
    if (location.line > line) {
        return false;
    }
    return location.column <= column;
}

enum CompletionItemKind {
    CompletionText = 1,
    CompletionMethod = 2,
    CompletionFunction = 3,
    CompletionField = 5,
    CompletionVariable = 6,
    CompletionModule = 9,
    CompletionEnum = 13,
    CompletionKeyword = 14,
    CompletionEnumMember = 20,
    CompletionStruct = 22
};

struct LocalSymbol {
    Type type = Type::void_type();
    Span definition;
    std::string hover;
};

struct GlobalContext {
    std::unordered_map<std::string, const StructDecl*> structs;
    std::unordered_map<std::string, const EnumDecl*> enums;
    std::unordered_map<std::string, const FunctionDecl*> functions;
    std::unordered_map<std::string, std::unordered_map<std::string, const FunctionDecl*>> methods;
    std::unordered_map<std::string, std::unordered_map<std::string, const StructField*>> fields;
    std::unordered_map<std::string, std::unordered_map<std::string, const EnumVariant*>> variants;
};

class SymbolIndexer {
  public:
    explicit SymbolIndexer(const FrontendResult& frontend) : frontend_(frontend) { build_globals(); }

    std::vector<SymbolEntry> build() {
        for (const auto& module : frontend_.modules) {
            for (const auto& import_decl : module.program.imports) {
                if (import_decl.kind != ImportKind::Module) {
                    continue;
                }
                const Span span = span_from_location(import_decl.module_name_location, import_decl.module_name.size());
                entries_.push_back({span, Span{resolve_module_target(import_decl.module_name), 1, 1, 1},
                                    "module " + import_decl.module_name});
            }
            for (const auto& enum_decl : module.program.enums) {
                index_enum(enum_decl);
            }
            for (const auto& struct_decl : module.program.structs) {
                index_struct(struct_decl);
            }
            for (const auto& function_decl : module.program.functions) {
                index_function(function_decl);
            }
        }
        return entries_;
    }

  private:
    void build_globals() {
        for (const auto& module : frontend_.modules) {
            for (const auto& struct_decl : module.program.structs) {
                globals_.structs[struct_decl.name] = &struct_decl;
                for (const auto& field : struct_decl.fields) {
                    globals_.fields[struct_decl.name][field.name] = &field;
                }
            }
            for (const auto& enum_decl : module.program.enums) {
                globals_.enums[enum_decl.name] = &enum_decl;
                for (const auto& variant : enum_decl.variants) {
                    globals_.variants[enum_decl.name][variant.name] = &variant;
                }
            }
            for (const auto& function_decl : module.program.functions) {
                if (function_decl.is_method()) {
                    globals_.methods[function_decl.impl_target][function_decl.name] = &function_decl;
                } else {
                    globals_.functions[function_decl.name] = &function_decl;
                }
            }
        }
    }

    fs::path resolve_module_target(const std::string& module_name) const {
        fs::path path = frontend_.project.root / "src";
        std::string segment;
        for (std::size_t i = 0; i < module_name.size(); ++i) {
            if (i + 1 < module_name.size() && module_name[i] == ':' && module_name[i + 1] == ':') {
                path /= segment;
                segment.clear();
                ++i;
                continue;
            }
            segment.push_back(module_name[i]);
        }
        if (!segment.empty()) {
            path /= segment;
        }
        path += ".pg";
        return fs::weakly_canonical(path);
    }

    void push_scope() { scopes_.push_back({}); }
    void pop_scope() { scopes_.pop_back(); }

    void define_local(const std::string& name, const SourceLocation& location, const Type& type, const std::string& hover) {
        LocalSymbol symbol{type, span_from_location(location, name.size()), hover};
        scopes_.back()[name] = symbol;
        entries_.push_back({symbol.definition, symbol.definition, hover});
    }

    std::optional<LocalSymbol> lookup_local(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            const auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

    void index_enum(const EnumDecl& decl) {
        const Span decl_span = span_from_location(decl.name_location, decl.name.size());
        entries_.push_back({decl_span, decl_span, "enum " + decl.name});
        for (const auto& variant : decl.variants) {
            const Span variant_span = span_from_location(variant.location, variant.name.size());
            std::string hover = decl.name + "::" + variant.name;
            if (variant.payload_type.has_value()) {
                hover += "(" + type_name(*variant.payload_type) + ")";
            }
            entries_.push_back({variant_span, variant_span, hover});
        }
    }

    void index_struct(const StructDecl& decl) {
        const Span decl_span = span_from_location(decl.name_location, decl.name.size());
        entries_.push_back({decl_span, decl_span, "struct " + decl.name});
        for (const auto& field : decl.fields) {
            const Span field_span = span_from_location(field.location, field.name.size());
            entries_.push_back({field_span, field_span, field.name + ": " + type_name(field.type)});
        }
    }

    void index_function(const FunctionDecl& decl) {
        const Span decl_span = span_from_location(decl.name_location, decl.name.size());
        entries_.push_back({decl_span, decl_span, function_hover(decl)});
        push_scope();
        for (const auto& param : decl.params) {
            if (!param.name.empty()) {
                define_local(param.name, param.location, param.type, param.name + ": " + type_name(param.type));
            }
        }
        for (const auto& stmt : decl.body) {
            visit_stmt(*stmt);
        }
        pop_scope();
    }

    Type visit_expr(const Expr& expr) {
        if (dynamic_cast<const IntegerExpr*>(&expr) != nullptr) {
            return Type::int_type();
        }
        if (dynamic_cast<const StringExpr*>(&expr) != nullptr) {
            return Type::string_type();
        }
        if (dynamic_cast<const BoolExpr*>(&expr) != nullptr) {
            return Type::bool_type();
        }
        if (const auto* tuple_expr = dynamic_cast<const TupleExpr*>(&expr)) {
            std::vector<Type> elements;
            for (const auto& element : tuple_expr->elements) {
                elements.push_back(visit_expr(*element));
            }
            return Type::tuple_type(std::move(elements));
        }
        if (const auto* array_expr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
            return Type::array_type(array_expr->elements.empty() ? Type::void_type() : visit_expr(*array_expr->elements.front()),
                                    array_expr->elements.size());
        }
        if (const auto* vec_expr = dynamic_cast<const VecLiteralExpr*>(&expr)) {
            if (vec_expr->declared_element_type.has_value()) {
                return Type::vec_type(*vec_expr->declared_element_type);
            }
            return Type::vec_type(vec_expr->elements.empty() ? Type::void_type() : visit_expr(*vec_expr->elements.front()));
        }
        if (const auto* variable_expr = dynamic_cast<const VariableExpr*>(&expr)) {
            if (const auto local = lookup_local(variable_expr->name)) {
                entries_.push_back({span_from_location(variable_expr->location, variable_expr->name.size()), local->definition,
                                    local->hover});
                return local->type;
            }
            return Type::void_type();
        }
        if (const auto* enum_expr = dynamic_cast<const EnumValueExpr*>(&expr)) {
            const Span enum_span = span_from_location(enum_expr->location, enum_expr->enum_name.size());
            entries_.push_back({enum_span, enum_span, "enum " + enum_expr->enum_name});
            if (const auto variants = globals_.variants.find(enum_expr->enum_name); variants != globals_.variants.end()) {
                if (const auto found = variants->second.find(enum_expr->variant); found != variants->second.end()) {
                    const Span variant_span = span_from_location(enum_expr->variant_location, enum_expr->variant.size());
                    const Span definition = span_from_location(found->second->location, found->second->name.size());
                    std::string hover = enum_expr->enum_name + "::" + enum_expr->variant;
                    if (found->second->payload_type.has_value()) {
                        hover += "(" + type_name(*found->second->payload_type) + ")";
                    }
                    entries_.push_back({variant_span, definition, hover});
                }
            }
            if (enum_expr->payload) {
                visit_expr(*enum_expr->payload);
            }
            return Type::enum_type(enum_expr->enum_name);
        }
        if (const auto* struct_expr = dynamic_cast<const StructLiteralExpr*>(&expr)) {
            const Span struct_span = span_from_location(struct_expr->location, struct_expr->struct_name.size());
            entries_.push_back({struct_span, struct_span, "struct " + struct_expr->struct_name});
            for (const auto& field : struct_expr->fields) {
                visit_expr(*field.value);
                if (const auto fields = globals_.fields.find(struct_expr->struct_name); fields != globals_.fields.end()) {
                    if (const auto found = fields->second.find(field.name); found != fields->second.end()) {
                        entries_.push_back({span_from_location(field.location, field.name.size()),
                                            span_from_location(found->second->location, found->second->name.size()),
                                            field.name + ": " + type_name(found->second->type)});
                    }
                }
            }
            return Type::struct_type(struct_expr->struct_name);
        }
        if (const auto* field_expr = dynamic_cast<const FieldAccessExpr*>(&expr)) {
            const Type object_type = visit_expr(*field_expr->object);
            if (object_type.kind == TypeKind::Struct) {
                if (const auto fields = globals_.fields.find(object_type.name); fields != globals_.fields.end()) {
                    if (const auto found = fields->second.find(field_expr->field); found != fields->second.end()) {
                        entries_.push_back({span_from_location(field_expr->location, field_expr->field.size()),
                                            span_from_location(found->second->location, found->second->name.size()),
                                            field_expr->field + ": " + type_name(found->second->type)});
                        return found->second->type;
                    }
                }
            }
            return Type::void_type();
        }
        if (const auto* method_expr = dynamic_cast<const MethodCallExpr*>(&expr)) {
            const Type object_type = visit_expr(*method_expr->object);
            for (const auto& arg : method_expr->args) {
                visit_expr(*arg);
            }
            if (object_type.kind == TypeKind::Vec) {
                const Span span = span_from_location(method_expr->location, method_expr->method.size());
                if (method_expr->method == "len") {
                    entries_.push_back({span, std::nullopt, "Vec<" + type_name(*object_type.element_type) + ">.len() -> int"});
                    return Type::int_type();
                }
                if (method_expr->method == "push") {
                    entries_.push_back(
                        {span, std::nullopt, "Vec<" + type_name(*object_type.element_type) + ">.push(value) -> void"});
                    return Type::void_type();
                }
            }
            if (object_type.kind == TypeKind::Struct) {
                if (const auto methods = globals_.methods.find(object_type.name); methods != globals_.methods.end()) {
                    if (const auto found = methods->second.find(method_expr->method); found != methods->second.end()) {
                        entries_.push_back({span_from_location(method_expr->location, method_expr->method.size()),
                                            span_from_location(found->second->name_location, found->second->name.size()),
                                            function_hover(*found->second)});
                        return found->second->return_type;
                    }
                }
            }
            return Type::void_type();
        }
        if (const auto* index_expr = dynamic_cast<const IndexExpr*>(&expr)) {
            const Type object_type = visit_expr(*index_expr->object);
            visit_expr(*index_expr->index);
            if ((object_type.kind == TypeKind::Array || object_type.kind == TypeKind::Vec) && object_type.element_type) {
                return *object_type.element_type;
            }
            return Type::void_type();
        }
        if (const auto* unary_expr = dynamic_cast<const UnaryExpr*>(&expr)) {
            return visit_expr(*unary_expr->expr);
        }
        if (const auto* call_expr = dynamic_cast<const CallExpr*>(&expr)) {
            for (const auto& arg : call_expr->args) {
                visit_expr(*arg);
            }
            const Span span = span_from_location(call_expr->location, call_expr->callee.size());
            if (const auto function_it = globals_.functions.find(call_expr->callee); function_it != globals_.functions.end()) {
                entries_.push_back({span, span_from_location(function_it->second->name_location, function_it->second->name.size()),
                                    function_hover(*function_it->second)});
                return function_it->second->return_type;
            }
            if (call_expr->callee == "str::len") {
                entries_.push_back({span, std::nullopt, "str::len(string) -> int"});
                return Type::int_type();
            }
            if (call_expr->callee == "str::eq" || call_expr->callee == "str::starts_with" || call_expr->callee == "str::ends_with" ||
                call_expr->callee == "fs::exists") {
                entries_.push_back({span, std::nullopt, call_expr->callee});
                return Type::bool_type();
            }
            if (call_expr->callee == "io::println") {
                entries_.push_back({span, std::nullopt, "io::println(value) -> void"});
                return Type::void_type();
            }
            if (call_expr->callee == "env::get") {
                entries_.push_back({span, std::nullopt, "env::get(name: string) -> EnvResult"});
                return Type::enum_type("EnvResult");
            }
            if (call_expr->callee == "fs::read_to_string" || call_expr->callee == "fs::cwd") {
                entries_.push_back({span, std::nullopt, call_expr->callee});
                return Type::enum_type("FsResult");
            }
            if (call_expr->callee == "fs::write_string" || call_expr->callee == "fs::remove" || call_expr->callee == "fs::create_dir") {
                entries_.push_back({span, std::nullopt, call_expr->callee});
                return Type::enum_type("FsWriteResult");
            }
            return Type::void_type();
        }
        if (const auto* con_expr = dynamic_cast<const ConExpr*>(&expr)) {
            std::vector<Type> items;
            for (const auto& item : con_expr->items) {
                items.push_back(visit_expr(*item));
            }
            return Type::tuple_type(std::move(items));
        }
        if (const auto* binary_expr = dynamic_cast<const BinaryExpr*>(&expr)) {
            const Type left = visit_expr(*binary_expr->left);
            const Type right = visit_expr(*binary_expr->right);
            if (binary_expr->op == "==" || binary_expr->op == "!=" || binary_expr->op == "<" || binary_expr->op == "<=" ||
                binary_expr->op == ">" || binary_expr->op == ">=" || binary_expr->op == "&&" || binary_expr->op == "||") {
                return Type::bool_type();
            }
            if (left.kind == TypeKind::String || right.kind == TypeKind::String) {
                return Type::string_type();
            }
            return Type::int_type();
        }
        return Type::void_type();
    }

    void visit_stmt(const Stmt& stmt) {
        if (const auto* let_stmt = dynamic_cast<const LetStmt*>(&stmt)) {
            const Type inferred = visit_expr(*let_stmt->initializer);
            const Type final_type = let_stmt->declared_type.value_or(inferred);
            define_local(let_stmt->name, let_stmt->name_location, final_type, let_stmt->name + ": " + type_name(final_type));
            return;
        }
        if (const auto* tuple_let = dynamic_cast<const TupleLetStmt*>(&stmt)) {
            const Type tuple_type = visit_expr(*tuple_let->initializer);
            if (tuple_type.kind == TypeKind::Tuple) {
                for (std::size_t i = 0; i < tuple_let->names.size() && i < tuple_type.tuple_elements.size(); ++i) {
                    define_local(tuple_let->names[i], tuple_let->name_locations[i], tuple_type.tuple_elements[i],
                                 tuple_let->names[i] + ": " + type_name(tuple_type.tuple_elements[i]));
                }
            }
            return;
        }
        if (const auto* assign_stmt = dynamic_cast<const AssignStmt*>(&stmt)) {
            visit_expr(*assign_stmt->value);
            return;
        }
        if (const auto* field_assign = dynamic_cast<const FieldAssignStmt*>(&stmt)) {
            visit_expr(*field_assign->object);
            visit_expr(*field_assign->value);
            return;
        }
        if (const auto* index_assign = dynamic_cast<const IndexAssignStmt*>(&stmt)) {
            visit_expr(*index_assign->object);
            visit_expr(*index_assign->index);
            visit_expr(*index_assign->value);
            return;
        }
        if (const auto* expr_stmt = dynamic_cast<const ExprStmt*>(&stmt)) {
            visit_expr(*expr_stmt->expr);
            return;
        }
        if (const auto* return_stmt = dynamic_cast<const ReturnStmt*>(&stmt)) {
            if (return_stmt->value) {
                visit_expr(*return_stmt->value);
            }
            return;
        }
        if (const auto* if_stmt = dynamic_cast<const IfStmt*>(&stmt)) {
            visit_expr(*if_stmt->condition);
            push_scope();
            for (const auto& inner : if_stmt->then_body) {
                visit_stmt(*inner);
            }
            pop_scope();
            push_scope();
            for (const auto& inner : if_stmt->else_body) {
                visit_stmt(*inner);
            }
            pop_scope();
            return;
        }
        if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(&stmt)) {
            visit_expr(*while_stmt->condition);
            push_scope();
            for (const auto& inner : while_stmt->body) {
                visit_stmt(*inner);
            }
            pop_scope();
            return;
        }
        if (const auto* for_stmt = dynamic_cast<const ForStmt*>(&stmt)) {
            visit_expr(*for_stmt->start);
            visit_expr(*for_stmt->end);
            push_scope();
            define_local(for_stmt->name, for_stmt->name_location, Type::int_type(), for_stmt->name + ": int");
            for (const auto& inner : for_stmt->body) {
                visit_stmt(*inner);
            }
            pop_scope();
            return;
        }
        if (const auto* match_stmt = dynamic_cast<const MatchStmt*>(&stmt)) {
            const Type subject_type = visit_expr(*match_stmt->subject);
            for (const auto& arm : match_stmt->arms) {
                entries_.push_back({span_from_location(arm.enum_name_location, arm.enum_name.size()),
                                    span_from_location(arm.enum_name_location, arm.enum_name.size()), "enum " + arm.enum_name});
                if (const auto variants = globals_.variants.find(arm.enum_name); variants != globals_.variants.end()) {
                    if (const auto found = variants->second.find(arm.variant); found != variants->second.end()) {
                        entries_.push_back({span_from_location(arm.variant_location, arm.variant.size()),
                                            span_from_location(found->second->location, found->second->name.size()),
                                            arm.enum_name + "::" + arm.variant});
                    }
                }
                push_scope();
                if (arm.binding_name.has_value() && arm.binding_location.has_value() && subject_type.kind == TypeKind::Enum) {
                    if (const auto variants = globals_.variants.find(subject_type.name); variants != globals_.variants.end()) {
                        if (const auto found = variants->second.find(arm.variant); found != variants->second.end() &&
                            found->second->payload_type.has_value()) {
                            define_local(*arm.binding_name, *arm.binding_location, *found->second->payload_type,
                                         *arm.binding_name + ": " + type_name(*found->second->payload_type));
                        }
                    }
                }
                for (const auto& inner : arm.body) {
                    visit_stmt(*inner);
                }
                pop_scope();
            }
        }
    }

    const FrontendResult& frontend_;
    GlobalContext globals_;
    std::vector<SymbolEntry> entries_;
    std::vector<std::unordered_map<std::string, LocalSymbol>> scopes_;
};

class CompletionEngine {
  public:
    explicit CompletionEngine(const FrontendResult& frontend) : frontend_(frontend) { build_globals(); }

    std::vector<CompletionCandidate> complete(const fs::path& path, std::size_t line_zero, std::size_t character_zero,
                                              const std::string& text) {
        const CompletionContext context = classify_completion_context(text, line_zero, character_zero);
        std::vector<CompletionCandidate> items;
        std::unordered_set<std::string> seen;

        if (context.in_std_import && context.kind == CompletionContextKind::Plain) {
            add_std_module_candidates(items, seen);
            return filter_candidates(items, context.prefix);
        }

        switch (context.kind) {
            case CompletionContextKind::Plain:
                add_local_candidates(items, seen, path, line_zero, character_zero);
                add_global_candidates(items, seen);
                add_import_root_candidates(items, seen, path);
                add_std_module_candidates(items, seen);
                break;
            case CompletionContextKind::Member:
                add_member_candidates(items, seen, path, line_zero, character_zero, context.receiver);
                break;
            case CompletionContextKind::Namespace:
                add_namespace_candidates(items, seen, path, context);
                break;
        }

        return filter_candidates(items, context.prefix);
    }

  private:
    struct ModuleChildrenSource {
        fs::path root;
    };

    void build_globals() {
        for (const auto& module : frontend_.modules) {
            const std::string canonical = fs::weakly_canonical(module.path).string();
            modules_by_path_[canonical] = &module;
            for (const auto& import_decl : module.program.imports) {
                if (import_decl.kind == ImportKind::Module) {
                    imported_roots_by_file_[canonical].insert(split_module_name(import_decl.module_name).front());
                }
            }
            for (const auto& struct_decl : module.program.structs) {
                globals_.structs[struct_decl.name] = &struct_decl;
                for (const auto& field : struct_decl.fields) {
                    globals_.fields[struct_decl.name][field.name] = &field;
                }
            }
            for (const auto& enum_decl : module.program.enums) {
                globals_.enums[enum_decl.name] = &enum_decl;
                for (const auto& variant : enum_decl.variants) {
                    globals_.variants[enum_decl.name][variant.name] = &variant;
                }
            }
            for (const auto& function_decl : module.program.functions) {
                if (function_decl.is_method()) {
                    globals_.methods[function_decl.impl_target][function_decl.name] = &function_decl;
                } else {
                    globals_.functions[function_decl.name] = &function_decl;
                }
            }
        }
    }

    void add_candidate(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen, CompletionCandidate item) {
        const std::string key = item.label + "|" + item.detail;
        if (seen.insert(key).second) {
            if (item.insert_text.empty()) {
                item.insert_text = item.label;
            }
            items.push_back(std::move(item));
        }
    }

    void add_global_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen) {
        for (const auto& [name, function] : globals_.functions) {
            add_candidate(items, seen, {name, CompletionFunction, function_hover(*function), name});
        }
        for (const auto& [name, struct_decl] : globals_.structs) {
            add_candidate(items, seen, {name, CompletionStruct, "struct " + struct_decl->name, name});
        }
        for (const auto& [name, enum_decl] : globals_.enums) {
            add_candidate(items, seen, {name, CompletionEnum, "enum " + enum_decl->name, name});
        }
    }

    void add_std_module_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen) {
        add_candidate(items, seen, {"io", CompletionModule, "std module", "io"});
        add_candidate(items, seen, {"str", CompletionModule, "std module", "str"});
        add_candidate(items, seen, {"fs", CompletionModule, "std module", "fs"});
        add_candidate(items, seen, {"env", CompletionModule, "std module", "env"});
    }

    std::vector<LocalSymbol> collect_visible_locals(const fs::path& path, std::size_t line_zero, std::size_t character_zero) const {
        std::vector<LocalSymbol> locals;
        const std::string canonical = fs::weakly_canonical(path).string();
        const auto module_it = modules_by_path_.find(canonical);
        if (module_it == modules_by_path_.end()) {
            return locals;
        }
        const ParsedModule* module = module_it->second;
        const FunctionDecl* function = nullptr;
        for (const auto& candidate : module->program.functions) {
            if (!location_before_or_equal(candidate.location, path, line_zero, character_zero)) {
                continue;
            }
            if (!function || candidate.location.line > function->location.line ||
                (candidate.location.line == function->location.line && candidate.location.column >= function->location.column)) {
                function = &candidate;
            }
        }
        if (!function) {
            return locals;
        }
        std::unordered_map<std::string, LocalSymbol> map;
        for (const auto& param : function->params) {
            if (!param.name.empty()) {
                map[param.name] = {param.type, span_from_location(param.location, param.name.size()),
                                   param.name + ": " + type_name(param.type)};
            }
        }
        for (const auto& stmt : function->body) {
            collect_stmt_locals(*stmt, path, line_zero, character_zero, map);
        }
        for (const auto& [name, symbol] : map) {
            locals.push_back(symbol);
        }
        return locals;
    }

    void collect_stmt_locals(const Stmt& stmt, const fs::path& path, std::size_t line_zero, std::size_t character_zero,
                             std::unordered_map<std::string, LocalSymbol>& locals) const {
        if (!location_before_or_equal(stmt.location, path, line_zero, character_zero)) {
            return;
        }
        if (const auto* let_stmt = dynamic_cast<const LetStmt*>(&stmt)) {
            const Type inferred = infer_expr_type(*let_stmt->initializer, locals);
            const Type final_type = let_stmt->declared_type.value_or(inferred);
            locals[let_stmt->name] = {final_type, span_from_location(let_stmt->name_location, let_stmt->name.size()),
                                      let_stmt->name + ": " + type_name(final_type)};
            return;
        }
        if (const auto* tuple_let = dynamic_cast<const TupleLetStmt*>(&stmt)) {
            const Type tuple_type = infer_expr_type(*tuple_let->initializer, locals);
            if (tuple_type.kind == TypeKind::Tuple) {
                for (std::size_t i = 0; i < tuple_let->names.size() && i < tuple_type.tuple_elements.size(); ++i) {
                    locals[tuple_let->names[i]] = {tuple_type.tuple_elements[i],
                                                   span_from_location(tuple_let->name_locations[i], tuple_let->names[i].size()),
                                                   tuple_let->names[i] + ": " + type_name(tuple_type.tuple_elements[i])};
                }
            }
            return;
        }
        if (const auto* if_stmt = dynamic_cast<const IfStmt*>(&stmt)) {
            auto then_locals = locals;
            for (const auto& inner : if_stmt->then_body) {
                collect_stmt_locals(*inner, path, line_zero, character_zero, then_locals);
            }
            for (const auto& [name, value] : then_locals) {
                locals.try_emplace(name, value);
            }
            auto else_locals = locals;
            for (const auto& inner : if_stmt->else_body) {
                collect_stmt_locals(*inner, path, line_zero, character_zero, else_locals);
            }
            for (const auto& [name, value] : else_locals) {
                locals.try_emplace(name, value);
            }
            return;
        }
        if (const auto* while_stmt = dynamic_cast<const WhileStmt*>(&stmt)) {
            auto loop_locals = locals;
            for (const auto& inner : while_stmt->body) {
                collect_stmt_locals(*inner, path, line_zero, character_zero, loop_locals);
            }
            for (const auto& [name, value] : loop_locals) {
                locals.try_emplace(name, value);
            }
            return;
        }
        if (const auto* for_stmt = dynamic_cast<const ForStmt*>(&stmt)) {
            auto loop_locals = locals;
            loop_locals[for_stmt->name] = {Type::int_type(), span_from_location(for_stmt->name_location, for_stmt->name.size()),
                                           for_stmt->name + ": int"};
            for (const auto& inner : for_stmt->body) {
                collect_stmt_locals(*inner, path, line_zero, character_zero, loop_locals);
            }
            for (const auto& [name, value] : loop_locals) {
                locals.try_emplace(name, value);
            }
            return;
        }
        if (const auto* match_stmt = dynamic_cast<const MatchStmt*>(&stmt)) {
            const Type subject_type = infer_expr_type(*match_stmt->subject, locals);
            for (const auto& arm : match_stmt->arms) {
                auto arm_locals = locals;
                if (arm.binding_name.has_value() && arm.binding_location.has_value() && subject_type.kind == TypeKind::Enum) {
                    if (const auto variants = globals_.variants.find(subject_type.name); variants != globals_.variants.end()) {
                        if (const auto found = variants->second.find(arm.variant); found != variants->second.end() &&
                            found->second->payload_type.has_value()) {
                            arm_locals[*arm.binding_name] = {*found->second->payload_type,
                                                             span_from_location(*arm.binding_location, arm.binding_name->size()),
                                                             *arm.binding_name + ": " + type_name(*found->second->payload_type)};
                        }
                    }
                }
                for (const auto& inner : arm.body) {
                    collect_stmt_locals(*inner, path, line_zero, character_zero, arm_locals);
                }
                for (const auto& [name, value] : arm_locals) {
                    locals.try_emplace(name, value);
                }
            }
        }
    }

    Type infer_expr_type(const Expr& expr, const std::unordered_map<std::string, LocalSymbol>& locals) const {
        if (dynamic_cast<const IntegerExpr*>(&expr) != nullptr) {
            return Type::int_type();
        }
        if (dynamic_cast<const StringExpr*>(&expr) != nullptr) {
            return Type::string_type();
        }
        if (dynamic_cast<const BoolExpr*>(&expr) != nullptr) {
            return Type::bool_type();
        }
        if (const auto* tuple_expr = dynamic_cast<const TupleExpr*>(&expr)) {
            std::vector<Type> elements;
            for (const auto& element : tuple_expr->elements) {
                elements.push_back(infer_expr_type(*element, locals));
            }
            return Type::tuple_type(std::move(elements));
        }
        if (const auto* array_expr = dynamic_cast<const ArrayLiteralExpr*>(&expr)) {
            return Type::array_type(array_expr->elements.empty() ? Type::void_type() : infer_expr_type(*array_expr->elements.front(), locals),
                                    array_expr->elements.size());
        }
        if (const auto* vec_expr = dynamic_cast<const VecLiteralExpr*>(&expr)) {
            if (vec_expr->declared_element_type.has_value()) {
                return Type::vec_type(*vec_expr->declared_element_type);
            }
            return Type::vec_type(vec_expr->elements.empty() ? Type::void_type() : infer_expr_type(*vec_expr->elements.front(), locals));
        }
        if (const auto* variable_expr = dynamic_cast<const VariableExpr*>(&expr)) {
            if (const auto it = locals.find(variable_expr->name); it != locals.end()) {
                return it->second.type;
            }
            return Type::void_type();
        }
        if (const auto* enum_expr = dynamic_cast<const EnumValueExpr*>(&expr)) {
            return Type::enum_type(enum_expr->enum_name);
        }
        if (const auto* struct_expr = dynamic_cast<const StructLiteralExpr*>(&expr)) {
            return Type::struct_type(struct_expr->struct_name);
        }
        if (const auto* field_expr = dynamic_cast<const FieldAccessExpr*>(&expr)) {
            const Type object_type = infer_expr_type(*field_expr->object, locals);
            if (object_type.kind == TypeKind::Struct) {
                if (const auto fields = globals_.fields.find(object_type.name); fields != globals_.fields.end()) {
                    if (const auto field = fields->second.find(field_expr->field); field != fields->second.end()) {
                        return field->second->type;
                    }
                }
            }
            return Type::void_type();
        }
        if (const auto* method_expr = dynamic_cast<const MethodCallExpr*>(&expr)) {
            const Type object_type = infer_expr_type(*method_expr->object, locals);
            if (object_type.kind == TypeKind::Vec) {
                if (method_expr->method == "len") {
                    return Type::int_type();
                }
                return Type::void_type();
            }
            if (object_type.kind == TypeKind::Struct) {
                if (const auto methods = globals_.methods.find(object_type.name); methods != globals_.methods.end()) {
                    if (const auto method = methods->second.find(method_expr->method); method != methods->second.end()) {
                        return method->second->return_type;
                    }
                }
            }
            return Type::void_type();
        }
        if (const auto* index_expr = dynamic_cast<const IndexExpr*>(&expr)) {
            const Type object_type = infer_expr_type(*index_expr->object, locals);
            if ((object_type.kind == TypeKind::Array || object_type.kind == TypeKind::Vec) && object_type.element_type) {
                return *object_type.element_type;
            }
            return Type::void_type();
        }
        if (const auto* unary_expr = dynamic_cast<const UnaryExpr*>(&expr)) {
            return infer_expr_type(*unary_expr->expr, locals);
        }
        if (const auto* call_expr = dynamic_cast<const CallExpr*>(&expr)) {
            if (const auto function_it = globals_.functions.find(call_expr->callee); function_it != globals_.functions.end()) {
                return function_it->second->return_type;
            }
            if (call_expr->callee == "str::len") {
                return Type::int_type();
            }
            if (call_expr->callee == "str::eq" || call_expr->callee == "str::starts_with" || call_expr->callee == "str::ends_with" ||
                call_expr->callee == "fs::exists") {
                return Type::bool_type();
            }
            if (call_expr->callee == "env::get") {
                return Type::enum_type("EnvResult");
            }
            if (call_expr->callee == "fs::read_to_string" || call_expr->callee == "fs::cwd") {
                return Type::enum_type("FsResult");
            }
            if (call_expr->callee == "fs::write_string" || call_expr->callee == "fs::remove" || call_expr->callee == "fs::create_dir") {
                return Type::enum_type("FsWriteResult");
            }
            return Type::void_type();
        }
        if (const auto* con_expr = dynamic_cast<const ConExpr*>(&expr)) {
            std::vector<Type> items;
            for (const auto& item : con_expr->items) {
                items.push_back(infer_expr_type(*item, locals));
            }
            return Type::tuple_type(std::move(items));
        }
        if (const auto* binary_expr = dynamic_cast<const BinaryExpr*>(&expr)) {
            const Type left = infer_expr_type(*binary_expr->left, locals);
            const Type right = infer_expr_type(*binary_expr->right, locals);
            if (binary_expr->op == "==" || binary_expr->op == "!=" || binary_expr->op == "<" || binary_expr->op == "<=" ||
                binary_expr->op == ">" || binary_expr->op == ">=" || binary_expr->op == "&&" || binary_expr->op == "||") {
                return Type::bool_type();
            }
            if (left.kind == TypeKind::String || right.kind == TypeKind::String) {
                return Type::string_type();
            }
            return Type::int_type();
        }
        return Type::void_type();
    }

    Type resolve_receiver_type(const fs::path& path, std::size_t line_zero, std::size_t character_zero,
                               const std::string& receiver) const {
        const std::vector<LocalSymbol> locals = collect_visible_locals(path, line_zero, character_zero);
        std::unordered_map<std::string, LocalSymbol> local_map;
        for (const auto& local : locals) {
            const std::string name = local.hover.substr(0, local.hover.find(':'));
            local_map[name] = local;
        }
        std::stringstream stream(receiver);
        std::string segment;
        std::vector<std::string> segments;
        while (std::getline(stream, segment, '.')) {
            if (!segment.empty()) {
                segments.push_back(segment);
            }
        }
        if (segments.empty()) {
            return Type::void_type();
        }
        Type current = Type::void_type();
        if (const auto it = local_map.find(segments.front()); it != local_map.end()) {
            current = it->second.type;
        } else {
            return Type::void_type();
        }
        for (std::size_t i = 1; i < segments.size(); ++i) {
            if (current.kind != TypeKind::Struct) {
                return Type::void_type();
            }
            const auto fields = globals_.fields.find(current.name);
            if (fields == globals_.fields.end()) {
                return Type::void_type();
            }
            const auto field = fields->second.find(segments[i]);
            if (field == fields->second.end()) {
                return Type::void_type();
            }
            current = field->second->type;
        }
        return current;
    }

    void add_local_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen, const fs::path& path,
                              std::size_t line_zero, std::size_t character_zero) {
        for (const auto& local : collect_visible_locals(path, line_zero, character_zero)) {
            const std::string name = local.hover.substr(0, local.hover.find(':'));
            add_candidate(items, seen, {name, CompletionVariable, local.hover, name});
        }
    }

    void add_member_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen, const fs::path& path,
                               std::size_t line_zero, std::size_t character_zero, const std::string& receiver) {
        const Type receiver_type = resolve_receiver_type(path, line_zero, character_zero, receiver);
        if (receiver_type.kind == TypeKind::Struct) {
            if (const auto fields = globals_.fields.find(receiver_type.name); fields != globals_.fields.end()) {
                for (const auto& [name, field] : fields->second) {
                    add_candidate(items, seen, {name, CompletionField, name + ": " + type_name(field->type), name});
                }
            }
            if (const auto methods = globals_.methods.find(receiver_type.name); methods != globals_.methods.end()) {
                for (const auto& [name, method] : methods->second) {
                    add_candidate(items, seen, {name, CompletionMethod, function_hover(*method), name});
                }
            }
            return;
        }
        if (receiver_type.kind == TypeKind::Vec && receiver_type.element_type) {
            add_candidate(items, seen,
                          {"len", CompletionMethod, "Vec<" + type_name(*receiver_type.element_type) + ">.len() -> int", "len"});
            add_candidate(items, seen, {"push", CompletionMethod,
                                        "Vec<" + type_name(*receiver_type.element_type) + ">.push(value) -> void", "push"});
        }
    }

    void add_std_builtin_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen,
                                    const std::string& module_name) {
        if (module_name == "io") {
            add_candidate(items, seen, {"println", CompletionFunction, "io::println(value) -> void", "println"});
            return;
        }
        if (module_name == "str") {
            add_candidate(items, seen, {"len", CompletionFunction, "str::len(string) -> int", "len"});
            add_candidate(items, seen, {"eq", CompletionFunction, "str::eq(string, string) -> bool", "eq"});
            add_candidate(items, seen, {"starts_with", CompletionFunction, "str::starts_with(string, string) -> bool", "starts_with"});
            add_candidate(items, seen, {"ends_with", CompletionFunction, "str::ends_with(string, string) -> bool", "ends_with"});
            return;
        }
        if (module_name == "fs") {
            add_candidate(items, seen, {"read_to_string", CompletionFunction, "fs::read_to_string(path: string) -> FsResult",
                                        "read_to_string"});
            add_candidate(items, seen, {"write_string", CompletionFunction,
                                        "fs::write_string(path: string, contents: string) -> FsWriteResult", "write_string"});
            add_candidate(items, seen, {"exists", CompletionFunction, "fs::exists(path: string) -> bool", "exists"});
            add_candidate(items, seen, {"remove", CompletionFunction, "fs::remove(path: string) -> FsWriteResult", "remove"});
            add_candidate(items, seen, {"create_dir", CompletionFunction, "fs::create_dir(path: string) -> FsWriteResult",
                                        "create_dir"});
            add_candidate(items, seen, {"cwd", CompletionFunction, "fs::cwd() -> FsResult", "cwd"});
            return;
        }
        if (module_name == "env") {
            add_candidate(items, seen, {"get", CompletionFunction, "env::get(name: string) -> EnvResult", "get"});
        }
    }

    std::set<std::string> list_module_children(const fs::path& source_root, const std::vector<std::string>& prefix_segments) const {
        std::set<std::string> children;
        fs::path directory = source_root;
        for (const auto& segment : prefix_segments) {
            directory /= segment;
        }
        if (fs::exists(directory) && fs::is_directory(directory)) {
            for (const auto& entry : fs::directory_iterator(directory)) {
                if (entry.is_directory()) {
                    children.insert(entry.path().filename().string());
                } else if (entry.is_regular_file() && entry.path().extension() == ".pg") {
                    children.insert(entry.path().stem().string());
                }
            }
        } else if (prefix_segments.empty() && fs::exists(source_root) && fs::is_directory(source_root)) {
            for (const auto& entry : fs::directory_iterator(source_root)) {
                if (entry.is_directory()) {
                    children.insert(entry.path().filename().string());
                } else if (entry.is_regular_file() && entry.path().extension() == ".pg") {
                    children.insert(entry.path().stem().string());
                }
            }
        }
        return children;
    }

    void add_import_root_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen, const fs::path& path) {
        const std::string canonical = fs::weakly_canonical(path).string();
        if (const auto imported = imported_roots_by_file_.find(canonical); imported != imported_roots_by_file_.end()) {
            for (const auto& root : imported->second) {
                add_candidate(items, seen, {root, CompletionModule, "imported module", root});
            }
        }
        for (const auto& child : list_module_children(frontend_.project.root / "src", {})) {
            add_candidate(items, seen, {child, CompletionModule, "module", child});
        }
        for (const auto& dependency : frontend_.project.dependencies) {
            add_candidate(items, seen, {dependency.name, CompletionModule, "dependency module root", dependency.name});
        }
    }

    void add_module_path_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen,
                                    const std::string& receiver) {
        const std::vector<std::string> segments = split_module_name(receiver);
        if (segments.empty()) {
            return;
        }
        if (const auto dep = std::find_if(frontend_.project.dependencies.begin(), frontend_.project.dependencies.end(),
                                          [&](const DependencyConfig& dependency) { return dependency.name == segments.front(); });
            dep != frontend_.project.dependencies.end()) {
            for (const auto& child : list_module_children(dep->resolved_path / "src", std::vector<std::string>(segments.begin() + 1, segments.end()))) {
                add_candidate(items, seen, {child, CompletionModule, "module", child});
            }
            return;
        }
        for (const auto& child : list_module_children(frontend_.project.root / "src", segments)) {
            add_candidate(items, seen, {child, CompletionModule, "module", child});
        }
    }

    void add_namespace_candidates(std::vector<CompletionCandidate>& items, std::unordered_set<std::string>& seen, const fs::path& path,
                                  const CompletionContext& context) {
        if (context.receiver == "io" || context.receiver == "str" || context.receiver == "fs" || context.receiver == "env") {
            add_std_builtin_candidates(items, seen, context.receiver);
            return;
        }
        if (const auto enum_it = globals_.enums.find(context.receiver); enum_it != globals_.enums.end()) {
            if (const auto variants = globals_.variants.find(context.receiver); variants != globals_.variants.end()) {
                for (const auto& [name, variant] : variants->second) {
                    std::string detail = context.receiver + "::" + name;
                    if (variant->payload_type.has_value()) {
                        detail += "(" + type_name(*variant->payload_type) + ")";
                    }
                    add_candidate(items, seen, {name, CompletionEnumMember, detail, name});
                }
            }
            return;
        }
        if (context.in_import) {
            if (context.receiver.empty()) {
                add_import_root_candidates(items, seen, path);
            } else {
                add_module_path_candidates(items, seen, context.receiver);
            }
        }
    }

    std::vector<CompletionCandidate> filter_candidates(const std::vector<CompletionCandidate>& items, const std::string& prefix) const {
        if (prefix.empty()) {
            return items;
        }
        std::vector<CompletionCandidate> filtered;
        for (const auto& item : items) {
            if (starts_with(item.label, prefix)) {
                filtered.push_back(item);
            }
        }
        return filtered;
    }

    const FrontendResult& frontend_;
    GlobalContext globals_;
    std::unordered_map<std::string, const ParsedModule*> modules_by_path_;
    std::unordered_map<std::string, std::set<std::string>> imported_roots_by_file_;
};

JsonObject make_diagnostic(const fs::path& path, const SourceLocation& location, const std::string& message) {
    const Span span{path, location.line, location.column, 1};
    return {{"uri", path_to_uri(path)},
            {"diagnostic",
             JsonObject{{"range", make_range(span)}, {"severity", 1}, {"source", "puff"}, {"message", message}}}};
}

AnalysisResult analyze_project(const fs::path& project_root, const std::unordered_map<std::string, std::string>& source_overrides) {
    AnalysisResult result;
    try {
        const FrontendResult frontend = load_frontend_project(project_root, std::nullopt, source_overrides);
        for (const auto& module : frontend.modules) {
            result.touched_files.push_back(fs::weakly_canonical(module.path));
        }
        SymbolIndexer indexer(frontend);
        result.symbols = indexer.build();
    } catch (const CompileError& error) {
        if (!error.location().file.empty()) {
            const fs::path path = fs::path(error.location().file);
            result.touched_files.push_back(path);
            result.diagnostics.push_back(make_diagnostic(path, error.location(), error.what()));
        }
    }
    return result;
}

int LspServer::run() {
    bool running = true;
    while (running) {
        const auto message = read_message();
        if (!message.has_value()) {
            break;
        }
        running = handle_message(*message);
    }
    return exit_code_;
}

std::optional<JsonObject> LspServer::read_message() {
    std::string header;
    std::size_t content_length = 0;
    while (std::getline(std::cin, header)) {
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
        if (header.empty()) {
            break;
        }
        const std::string prefix = "Content-Length:";
        if (header.rfind(prefix, 0) == 0) {
            content_length = static_cast<std::size_t>(std::stoul(header.substr(prefix.size())));
        }
    }
    if (content_length == 0) {
        return std::nullopt;
    }
    std::string payload(content_length, '\0');
    std::cin.read(payload.data(), static_cast<std::streamsize>(content_length));
    JsonValue parsed = JsonParser(payload).parse();
    return parsed.as_object();
}

void LspServer::send_json(const JsonObject& object) {
    const std::string payload = stringify_json(JsonValue(object));
    std::cout << "Content-Length: " << payload.size() << "\r\n\r\n" << payload;
    std::cout.flush();
}

void LspServer::send_response(const JsonValue& id, JsonValue result) {
    send_json({{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}});
}

void LspServer::send_error(const JsonValue& id, int code, const std::string& message) {
    send_json({{"jsonrpc", "2.0"},
               {"id", id},
               {"error", JsonObject{{"code", code}, {"message", message}}}});
}

void LspServer::publish_diagnostics(const AnalysisResult& analysis) {
    std::unordered_map<std::string, JsonArray> by_uri;
    for (const auto& path : analysis.touched_files) {
        by_uri[path_to_uri(path)];
    }
    for (const auto& diagnostic : analysis.diagnostics) {
        const std::string uri = diagnostic.at("uri").as_string();
        by_uri[uri].push_back(diagnostic.at("diagnostic"));
    }
    for (const auto& [uri, diagnostics] : by_uri) {
        send_json({{"jsonrpc", "2.0"},
                   {"method", "textDocument/publishDiagnostics"},
                   {"params", JsonObject{{"uri", uri}, {"diagnostics", diagnostics}}}});
    }
}

std::unordered_map<std::string, std::string> LspServer::overrides_for_project(const fs::path& project_root) const {
    std::unordered_map<std::string, std::string> overrides;
    const std::string prefix = fs::weakly_canonical(project_root).string();
    for (const auto& [uri, text] : open_documents_) {
        const std::string path = fs::weakly_canonical(uri_to_path(uri)).string();
        if (path.rfind(prefix, 0) == 0) {
            overrides[path] = text;
        }
    }
    return overrides;
}

AnalysisResult LspServer::analyze_document_project(const std::string& uri) {
    const fs::path path = uri_to_path(uri);
    const fs::path project_root = find_project_root(path);
    return analyze_project(project_root, overrides_for_project(project_root));
}

std::optional<FrontendResult> load_completion_frontend(const fs::path& project_root,
                                                       const std::unordered_map<std::string, std::string>& source_overrides) {
    try {
        return load_frontend_project(project_root, std::nullopt, source_overrides);
    } catch (...) {
        try {
            return load_frontend_project(project_root);
        } catch (...) {
            return std::nullopt;
        }
    }
}

const SymbolEntry* LspServer::find_symbol_at(const std::vector<SymbolEntry>& symbols, const fs::path& path, std::size_t line,
                                             std::size_t character) const {
    const std::string canonical = fs::weakly_canonical(path).string();
    for (const auto& symbol : symbols) {
        if (fs::weakly_canonical(symbol.span.path).string() != canonical) {
            continue;
        }
        if (span_contains(symbol.span, line, character)) {
            return &symbol;
        }
    }
    return nullptr;
}

bool LspServer::handle_message(const JsonObject& message) {
    const JsonValue* id = object_get(message, "id");
    const std::optional<std::string> method = object_get_string(message, "method");
    const JsonObject empty;
    const JsonObject params = object_get(message, "params") && object_get(message, "params")->is_object()
                                  ? object_get(message, "params")->as_object()
                                  : empty;

    if (method == "initialize") {
        send_response(id ? *id : JsonValue(nullptr),
                      JsonObject{{"capabilities",
                                  JsonObject{{"textDocumentSync", 1},
                                             {"definitionProvider", true},
                                             {"hoverProvider", true},
                                             {"completionProvider",
                                              JsonObject{{"triggerCharacters", JsonArray{".", ":"}}}}}}});
        return true;
    }
    if (method == "initialized") {
        return true;
    }
    if (method == "shutdown") {
        shutdown_requested_ = true;
        send_response(id ? *id : JsonValue(nullptr), JsonValue(nullptr));
        return true;
    }
    if (method == "exit") {
        exit_code_ = shutdown_requested_ ? 0 : 1;
        return false;
    }
    if (method == "textDocument/didOpen" || method == "textDocument/didChange" || method == "textDocument/didSave") {
        handle_document_sync(*method, params);
        return true;
    }
    if (method == "textDocument/didClose") {
        if (const JsonValue* text_document = object_get(params, "textDocument"); text_document && text_document->is_object()) {
            if (const auto uri = object_get_string(text_document->as_object(), "uri")) {
                open_documents_.erase(*uri);
                send_json({{"jsonrpc", "2.0"},
                           {"method", "textDocument/publishDiagnostics"},
                           {"params", JsonObject{{"uri", *uri}, {"diagnostics", JsonArray{}}}}});
            }
        }
        return true;
    }
    if (method == "textDocument/definition") {
        if (!id) {
            return true;
        }
        send_response(*id, handle_definition(params));
        return true;
    }
    if (method == "textDocument/hover") {
        if (!id) {
            return true;
        }
        send_response(*id, handle_hover(params));
        return true;
    }
    if (method == "textDocument/completion") {
        if (!id) {
            return true;
        }
        send_response(*id, handle_completion(params));
        return true;
    }
    if (id) {
        send_response(*id, JsonValue(nullptr));
    }
    return true;
}

void LspServer::handle_document_sync(const std::string& method, const JsonObject& params) {
    const JsonValue* text_document = object_get(params, "textDocument");
    if (!text_document || !text_document->is_object()) {
        return;
    }
    const auto uri = object_get_string(text_document->as_object(), "uri");
    if (!uri.has_value()) {
        return;
    }
    if (method == "textDocument/didOpen") {
        if (const auto text = object_get_string(text_document->as_object(), "text")) {
            open_documents_[*uri] = *text;
        }
    } else if (method == "textDocument/didChange") {
        if (const JsonValue* changes = object_get(params, "contentChanges"); changes && changes->is_array() &&
            !changes->as_array().empty() && changes->as_array().front().is_object()) {
            if (const auto text = object_get_string(changes->as_array().front().as_object(), "text")) {
                open_documents_[*uri] = *text;
            }
        }
    }
    try {
        publish_diagnostics(analyze_document_project(*uri));
    } catch (...) {
    }
}

JsonValue LspServer::handle_definition(const JsonObject& params) {
    const JsonValue* text_document = object_get(params, "textDocument");
    const JsonValue* position = object_get(params, "position");
    if (!text_document || !position || !text_document->is_object() || !position->is_object()) {
        return JsonValue(nullptr);
    }
    const auto uri = object_get_string(text_document->as_object(), "uri");
    const auto line = object_get_int(position->as_object(), "line");
    const auto character = object_get_int(position->as_object(), "character");
    if (!uri || !line || !character) {
        return JsonValue(nullptr);
    }
    const AnalysisResult analysis = analyze_document_project(*uri);
    const SymbolEntry* symbol = find_symbol_at(analysis.symbols, uri_to_path(*uri), *line, *character);
    if (!symbol || !symbol->definition.has_value()) {
        return JsonValue(nullptr);
    }
    return JsonObject(make_location(*symbol->definition));
}

JsonValue LspServer::handle_hover(const JsonObject& params) {
    const JsonValue* text_document = object_get(params, "textDocument");
    const JsonValue* position = object_get(params, "position");
    if (!text_document || !position || !text_document->is_object() || !position->is_object()) {
        return JsonValue(nullptr);
    }
    const auto uri = object_get_string(text_document->as_object(), "uri");
    const auto line = object_get_int(position->as_object(), "line");
    const auto character = object_get_int(position->as_object(), "character");
    if (!uri || !line || !character) {
        return JsonValue(nullptr);
    }
    const AnalysisResult analysis = analyze_document_project(*uri);
    const SymbolEntry* symbol = find_symbol_at(analysis.symbols, uri_to_path(*uri), *line, *character);
    if (!symbol) {
        return JsonValue(nullptr);
    }
    return JsonObject{{"contents", JsonObject{{"kind", "markdown"}, {"value", "```pg\n" + symbol->hover + "\n```"}}}};
}

JsonValue LspServer::handle_completion(const JsonObject& params) {
    const JsonValue* text_document = object_get(params, "textDocument");
    const JsonValue* position = object_get(params, "position");
    if (!text_document || !position || !text_document->is_object() || !position->is_object()) {
        return JsonArray{};
    }
    const auto uri = object_get_string(text_document->as_object(), "uri");
    const auto line = object_get_int(position->as_object(), "line");
    const auto character = object_get_int(position->as_object(), "character");
    if (!uri || !line || !character) {
        return JsonArray{};
    }

    const fs::path path = uri_to_path(*uri);
    const fs::path project_root = find_project_root(path);
    const auto overrides = overrides_for_project(project_root);
    const auto frontend = load_completion_frontend(project_root, overrides);
    if (!frontend.has_value()) {
        return JsonArray{};
    }

    std::string text;
    if (const auto open = open_documents_.find(*uri); open != open_documents_.end()) {
        text = open->second;
    } else {
        try {
            text = read_file(path);
        } catch (...) {
            text.clear();
        }
    }

    CompletionEngine engine(*frontend);
    const auto completions = engine.complete(path, static_cast<std::size_t>(*line), static_cast<std::size_t>(*character), text);
    JsonArray items;
    for (const auto& item : completions) {
        items.push_back(JsonObject{{"label", item.label},
                                   {"kind", item.kind},
                                   {"detail", item.detail},
                                   {"insertText", item.insert_text}});
    }
    return items;
}

}  // namespace

int command_lsp() {
    LspServer server;
    return server.run();
}

}  // namespace pinggen

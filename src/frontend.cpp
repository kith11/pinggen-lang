#include "pinggen/frontend.hpp"

#include <fstream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

#include "pinggen/dependency_resolver.hpp"
#include "pinggen/diagnostics.hpp"
#include "pinggen/lexer.hpp"
#include "pinggen/parser.hpp"
#include "pinggen/sema.hpp"

namespace fs = std::filesystem;

namespace pinggen {

namespace {

struct LoadedProject {
    ProjectConfig config;
    std::unordered_map<std::string, std::shared_ptr<LoadedProject>> dependencies;
};

std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read source file '" + path.string() + "'");
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
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

fs::path project_module_path(const LoadedProject& project, const std::string& module_name) {
    fs::path path = project.config.root / "src";
    for (const auto& segment : split_module_name(module_name)) {
        path /= segment;
    }
    path += ".pg";
    return path;
}

std::string module_name_from_source_path(const LoadedProject& project, const fs::path& path) {
    const fs::path source_root = project.config.root / "src";
    std::error_code error;
    fs::path relative = fs::relative(path, source_root, error);
    if (error || relative.empty()) {
        return path.stem().string();
    }
    relative.replace_extension();
    std::string module_name;
    for (const auto& segment : relative) {
        if (!module_name.empty()) {
            module_name += "::";
        }
        module_name += segment.string();
    }
    return module_name.empty() ? path.stem().string() : module_name;
}

std::shared_ptr<LoadedProject> load_project_graph(
    const fs::path& project_dir, std::unordered_map<std::string, std::shared_ptr<LoadedProject>>& cache,
    std::unordered_set<std::string>& active_roots, const std::optional<std::string>& inherited_registry_index = std::nullopt) {
    const std::string canonical_root = fs::weakly_canonical(project_dir).string();
    if (active_roots.contains(canonical_root)) {
        throw std::runtime_error("circular project dependency detected at '" + canonical_root + "'");
    }
    if (const auto it = cache.find(canonical_root); it != cache.end()) {
        return it->second;
    }

    active_roots.insert(canonical_root);
    auto loaded = std::make_shared<LoadedProject>();
    loaded->config =
        resolve_registry_dependencies(load_project(project_dir, inherited_registry_index.has_value()), inherited_registry_index);
    cache.emplace(canonical_root, loaded);

    const std::optional<std::string> effective_registry =
        loaded->config.registry.index.has_value() ? loaded->config.registry.index : inherited_registry_index;
    for (const auto& dependency : loaded->config.dependencies) {
        loaded->dependencies.emplace(
            dependency.name, load_project_graph(dependency.resolved_path, cache, active_roots, effective_registry));
    }

    active_roots.erase(canonical_root);
    return loaded;
}

struct ModuleResolution {
    std::shared_ptr<LoadedProject> project;
    std::string local_module_name;
    std::string canonical_module_name;
    fs::path path;
};

ModuleResolution resolve_import(const std::shared_ptr<LoadedProject>& project, const std::string& namespace_prefix,
                                const std::string& import_name, const SourceLocation& location) {
    const std::vector<std::string> segments = split_module_name(import_name);
    if (!segments.empty()) {
        const auto dependency_it = project->dependencies.find(segments[0]);
        if (dependency_it != project->dependencies.end()) {
            if (segments.size() == 1) {
                fail(location, "dependency imports must include a module path after '" + segments[0] + "::'");
            }
            const std::shared_ptr<LoadedProject>& dependency_project = dependency_it->second;
            const std::string dependency_prefix =
                namespace_prefix.empty() ? segments[0] : namespace_prefix + "::" + segments[0];
            const std::string local_module_name = join_module_segments(segments, 1);
            const std::string canonical_module_name = dependency_prefix + "::" + local_module_name;
            return {dependency_project, local_module_name, canonical_module_name,
                    project_module_path(*dependency_project, local_module_name)};
        }
    }

    const std::string canonical_module_name =
        namespace_prefix.empty() ? import_name : namespace_prefix + "::" + import_name;
    return {project, import_name, canonical_module_name, project_module_path(*project, import_name)};
}

ParsedModule parse_module(const fs::path& path, const std::unordered_map<std::string, std::string>& source_overrides) {
    const std::string canonical_path = fs::weakly_canonical(path).string();
    const auto override = source_overrides.find(canonical_path);
    const std::string source = override != source_overrides.end() ? override->second : read_file(path);
    Lexer lexer(source, canonical_path);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    Program program = parser.parse();
    return {path, "", std::move(tokens), std::move(program)};
}

void append_program(Program& target, Program source) {
    for (auto& import_decl : source.imports) {
        target.imports.push_back(std::move(import_decl));
    }
    for (auto& enum_decl : source.enums) {
        target.enums.push_back(std::move(enum_decl));
    }
    for (auto& struct_decl : source.structs) {
        target.structs.push_back(std::move(struct_decl));
    }
    for (auto& function_decl : source.functions) {
        target.functions.push_back(std::move(function_decl));
    }
}

void load_module_graph(const std::shared_ptr<LoadedProject>& project, const std::string& namespace_prefix, const fs::path& path,
                       const std::string& module_name, FrontendResult& result,
                       std::unordered_set<std::string>& loaded_modules, std::unordered_set<std::string>& active_modules,
                       const std::unordered_map<std::string, std::string>& source_overrides) {
    ParsedModule parsed = parse_module(path, source_overrides);
    parsed.module_name = module_name;
    active_modules.insert(module_name);
    for (const auto& import_decl : parsed.program.imports) {
        if (import_decl.kind != ImportKind::Module) {
            continue;
        }
        const ModuleResolution resolved = resolve_import(project, namespace_prefix, import_decl.module_name, import_decl.location);
        if (active_modules.contains(resolved.canonical_module_name)) {
            fail(import_decl.location, "circular module import detected for '" + resolved.canonical_module_name + "'");
        }
        if (loaded_modules.contains(resolved.canonical_module_name)) {
            continue;
        }
        if (!fs::exists(resolved.path)) {
            fail(import_decl.location,
                 "missing imported module '" + resolved.canonical_module_name + "'; expected file '" + resolved.path.string() + "'");
        }
        loaded_modules.insert(resolved.canonical_module_name);
        const std::string child_prefix =
            resolved.canonical_module_name.substr(0, resolved.canonical_module_name.size() - resolved.local_module_name.size() -
                                                  (resolved.canonical_module_name.size() == resolved.local_module_name.size() ? 0 : 2));
        load_module_graph(resolved.project, child_prefix, resolved.path, resolved.canonical_module_name, result, loaded_modules,
                          active_modules, source_overrides);
    }
    active_modules.erase(module_name);
    ParsedModule module_copy = parse_module(path, source_overrides);
    module_copy.module_name = module_name;
    append_program(result.merged_program, std::move(parsed.program));
    result.modules.push_back(std::move(module_copy));
}

}  // namespace

fs::path find_project_root(const fs::path& source_path) {
    fs::path current = fs::absolute(source_path);
    if (fs::is_regular_file(current)) {
        current = current.parent_path();
    }
    while (!current.empty()) {
        if (fs::exists(current / "pinggen.toml")) {
            return current;
        }
        const fs::path parent = current.parent_path();
        if (parent == current) {
            break;
        }
        current = parent;
    }
    throw std::runtime_error("could not find puff project root for '" + source_path.string() + "'");
}

FrontendResult load_frontend_project(const fs::path& project_dir, const std::optional<std::string>& target_name,
                                     const std::unordered_map<std::string, std::string>& source_overrides) {
    FrontendResult result;
    result.project = load_project(project_dir);
    result.target = resolve_target(result.project, target_name);

    std::unordered_map<std::string, std::shared_ptr<LoadedProject>> cache;
    std::unordered_set<std::string> active_roots;
    const std::shared_ptr<LoadedProject> root_project = load_project_graph(result.project.root, cache, active_roots);

    std::unordered_set<std::string> loaded_modules;
    std::unordered_set<std::string> active_modules;
    const std::string entry_module_name = module_name_from_source_path(*root_project, result.target.entry);
    loaded_modules.insert(entry_module_name);
    load_module_graph(root_project, "", result.target.entry, entry_module_name, result, loaded_modules, active_modules,
                      source_overrides);

    SemanticAnalyzer sema;
    sema.analyze(result.merged_program);
    return result;
}

}  // namespace pinggen

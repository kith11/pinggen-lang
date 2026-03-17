#include "pinggen/project.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "pinggen/diagnostics.hpp"

namespace pinggen {

namespace {

struct RawTargetConfig {
    std::string name;
    std::string entry;
    std::string output;
};

struct RawDependencyConfig {
    std::string name;
    std::string path;
    std::string version;
};

std::string trim(const std::string& value) {
    const auto start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const auto end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

std::string unquote(const std::string& value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
        return value.substr(1, value.size() - 2);
    }
    return value;
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

BuildTarget normalize_target(const std::filesystem::path& root, const RawTargetConfig& raw, const std::string& fallback_name,
                             const std::filesystem::path& fallback_output_dir, const std::filesystem::path& config_path,
                             const std::string& section_name) {
    BuildTarget target;
    target.name = raw.name.empty() ? fallback_name : raw.name;
    if (target.name.empty()) {
        throw std::runtime_error("project config '" + config_path.string() + "' is missing " + section_name + ".name");
    }
    if (raw.entry.empty()) {
        throw std::runtime_error("project config '" + config_path.string() + "' is missing " + section_name + ".entry");
    }
    target.entry = root / raw.entry;
    if (raw.output.empty()) {
        target.output = fallback_output_dir / target.name;
    } else {
        target.output = root / raw.output;
    }
    if (!std::filesystem::exists(target.entry)) {
        throw std::runtime_error("project entry file not found: " + target.entry.string() + " (from " + section_name +
                                 ".entry in " + config_path.string() + ")");
    }
    return target;
}

}  // namespace

ProjectConfig load_project(const std::filesystem::path& project_dir, bool allow_inherited_registry) {
    const auto config_path = project_dir / "pinggen.toml";
    std::ifstream input(config_path);
    if (!input) {
        throw std::runtime_error("missing project config '" + config_path.string() + "'");
    }

    ProjectConfig config;
    config.root = std::filesystem::absolute(project_dir);
    std::string section;
    std::string line;
    RawTargetConfig build_section;
    std::vector<RawTargetConfig> raw_targets;
    std::vector<RawDependencyConfig> raw_dependencies;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            if (section == "[target]") {
                raw_targets.push_back(RawTargetConfig{});
            }
            if (section == "[dependency]") {
                raw_dependencies.push_back(RawDependencyConfig{});
            }
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = unquote(trim(line.substr(eq + 1)));
        if (section == "package" && key == "name") {
            config.name = value;
        } else if (section == "registry" && key == "index") {
            config.registry.index = value;
        } else if (section == "build" && key == "name") {
            build_section.name = value;
        } else if (section == "build" && key == "entry") {
            build_section.entry = value;
        } else if (section == "build" && key == "output") {
            build_section.output = value;
        } else if (section == "[target]") {
            auto& target = raw_targets.back();
            if (key == "name") {
                target.name = value;
            } else if (key == "entry") {
                target.entry = value;
            } else if (key == "output") {
                target.output = value;
            }
        } else if (section == "[dependency]") {
            auto& dependency = raw_dependencies.back();
            if (key == "name") {
                dependency.name = value;
            } else if (key == "path") {
                dependency.path = value;
            } else if (key == "version") {
                dependency.version = value;
            }
        }
    }

    if (config.name.empty()) {
        throw std::runtime_error("project config '" + config_path.string() + "' is missing [package].name");
    }
    if (config.registry.index.has_value()) {
        const std::filesystem::path index_path(*config.registry.index);
        if (!starts_with(*config.registry.index, "http://") && !starts_with(*config.registry.index, "https://") &&
            !starts_with(*config.registry.index, "file://") && !index_path.is_absolute()) {
            config.registry.index = (config.root / index_path).lexically_normal().string();
        }
    }
    if (build_section.entry.empty()) {
        throw std::runtime_error("project config '" + config_path.string() + "' is missing [build].entry");
    }

    const std::filesystem::path default_output_dir = config.root / "build";
    config.default_target =
        normalize_target(config.root, build_section, config.name, default_output_dir, config_path, "[build]");

    std::unordered_set<std::string> seen_target_names;
    seen_target_names.insert(config.default_target.name);
    for (std::size_t i = 0; i < raw_targets.size(); ++i) {
        const auto& raw_target = raw_targets[i];
        const BuildTarget target =
            normalize_target(config.root, raw_target, "", default_output_dir, config_path,
                             "[[target]] #" + std::to_string(i + 1));
        if (!seen_target_names.insert(target.name).second) {
            throw std::runtime_error("project config '" + config_path.string() + "' defines duplicate target '" +
                                     target.name + "'");
        }
        config.extra_targets.push_back(target);
    }

    std::unordered_set<std::string> seen_dependency_names;
    for (std::size_t i = 0; i < raw_dependencies.size(); ++i) {
        const auto& raw_dependency = raw_dependencies[i];
        const std::string section_name = "[[dependency]] #" + std::to_string(i + 1);
        if (raw_dependency.name.empty()) {
            throw std::runtime_error("project config '" + config_path.string() + "' is missing " + section_name + ".name");
        }
        const bool has_path = !raw_dependency.path.empty();
        const bool has_version = !raw_dependency.version.empty();
        if (has_path == has_version) {
            throw std::runtime_error("project config '" + config_path.string() + "' must define exactly one of " +
                                     section_name + ".path or " + section_name + ".version");
        }
        if (raw_dependency.name == "std") {
            throw std::runtime_error("project config '" + config_path.string() +
                                     "' uses reserved dependency name 'std'");
        }
        if (raw_dependency.name.find("::") != std::string::npos) {
            throw std::runtime_error("project config '" + config_path.string() + "' has invalid dependency name '" +
                                     raw_dependency.name + "'; dependency names must be a single identifier");
        }
        if (!seen_dependency_names.insert(raw_dependency.name).second) {
            throw std::runtime_error("project config '" + config_path.string() + "' defines duplicate dependency '" +
                                     raw_dependency.name + "'");
        }

        DependencyConfig dependency;
        dependency.name = raw_dependency.name;
        if (has_path) {
            dependency.source_kind = DependencySourceKind::Path;
            dependency.path = std::filesystem::absolute(config.root / raw_dependency.path);
            dependency.resolved_path = *dependency.path;
            const auto dependency_config_path = dependency.resolved_path / "pinggen.toml";
            if (!std::filesystem::exists(dependency_config_path)) {
                throw std::runtime_error("dependency '" + dependency.name + "' config not found at '" +
                                         dependency_config_path.string() + "'");
            }
        } else {
            dependency.source_kind = DependencySourceKind::Registry;
            dependency.version_requirement = raw_dependency.version;
        }
        config.dependencies.push_back(std::move(dependency));
    }
    const bool has_registry_dependencies = std::any_of(
        config.dependencies.begin(), config.dependencies.end(),
        [](const DependencyConfig& dependency) { return dependency.source_kind == DependencySourceKind::Registry; });
    if (has_registry_dependencies && !config.registry.index.has_value() && !allow_inherited_registry) {
        throw std::runtime_error("project config '" + config_path.string() +
                                 "' must define [registry].index when any dependency uses .version");
    }
    return config;
}

const BuildTarget& resolve_target(const ProjectConfig& project, const std::optional<std::string>& name) {
    if (!name.has_value()) {
        return project.default_target;
    }
    if (*name == project.default_target.name) {
        return project.default_target;
    }
    for (const auto& target : project.extra_targets) {
        if (target.name == *name) {
            return target;
        }
    }
    throw std::runtime_error("unknown build target '" + *name + "' in project '" + project.name + "'");
}

void create_project(const std::filesystem::path& target_dir, const std::string& name) {
    std::filesystem::create_directories(target_dir / "src");
    std::filesystem::create_directories(target_dir / "build");

    std::ofstream config(target_dir / "pinggen.toml");
    config << "[package]\n";
    config << "name = \"" << name << "\"\n";
    config << "version = \"0.1.0\"\n\n";
    config << "[build]\n";
    config << "name = \"" << name << "\"\n";
    config << "entry = \"src/main.pg\"\n";
    config << "output = \"build/" << name << "\"\n";

    std::ofstream main_source(target_dir / "src" / "main.pg");
    main_source << "import std::{ io }\n";
    main_source << "import greet;\n\n";
    main_source << "func main() {\n";
    main_source << "    io::println(greeting(\"pinggen\"));\n";
    main_source << "}\n";

    std::ofstream greet_source(target_dir / "src" / "greet.pg");
    greet_source << "func greeting(name: string) -> string {\n";
    greet_source << "    return \"hello, \" + name;\n";
    greet_source << "}\n";
}

void add_registry_dependency(const std::filesystem::path& project_dir, const std::string& name, const std::string& version_requirement) {
    const ProjectConfig project = load_project(project_dir);
    if (!project.registry.index.has_value()) {
        throw std::runtime_error("project '" + project.name + "' must define [registry].index before using 'puff add'");
    }
    for (const auto& dependency : project.dependencies) {
        if (dependency.name == name) {
            throw std::runtime_error("project '" + project.name + "' already defines dependency '" + name + "'");
        }
    }

    const std::filesystem::path config_path = project.root / "pinggen.toml";
    std::ofstream config(config_path, std::ios::app);
    if (!config) {
        throw std::runtime_error("failed to update project config '" + config_path.string() + "'");
    }
    config << "\n[[dependency]]\n";
    config << "name = \"" << name << "\"\n";
    config << "version = \"" << version_requirement << "\"\n";
}

}  // namespace pinggen

#include "pinggen/project.hpp"

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

ProjectConfig load_project(const std::filesystem::path& project_dir) {
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
        }
    }

    if (config.name.empty()) {
        throw std::runtime_error("project config '" + config_path.string() + "' is missing [package].name");
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

}  // namespace pinggen

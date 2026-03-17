#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace pinggen {

struct BuildTarget {
    std::string name;
    std::filesystem::path entry;
    std::filesystem::path output;
};

enum class DependencySourceKind {
    Path,
    Registry
};

struct DependencyConfig {
    std::string name;
    DependencySourceKind source_kind = DependencySourceKind::Path;
    std::optional<std::filesystem::path> path;
    std::optional<std::string> version_requirement;
    std::optional<std::string> resolved_version;
    std::filesystem::path resolved_path;
};

struct RegistryConfig {
    std::optional<std::string> index;
};

struct ProjectConfig {
    std::string name;
    std::filesystem::path root;
    BuildTarget default_target;
    std::vector<BuildTarget> extra_targets;
    RegistryConfig registry;
    std::vector<DependencyConfig> dependencies;
};

ProjectConfig load_project(const std::filesystem::path& project_dir, bool allow_inherited_registry = false);
const BuildTarget& resolve_target(const ProjectConfig& project, const std::optional<std::string>& name);
void create_project(const std::filesystem::path& target_dir, const std::string& name);
void add_registry_dependency(const std::filesystem::path& project_dir, const std::string& name, const std::string& version_requirement);

}  // namespace pinggen

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

struct ProjectConfig {
    std::string name;
    std::filesystem::path root;
    BuildTarget default_target;
    std::vector<BuildTarget> extra_targets;
};

ProjectConfig load_project(const std::filesystem::path& project_dir);
const BuildTarget& resolve_target(const ProjectConfig& project, const std::optional<std::string>& name);
void create_project(const std::filesystem::path& target_dir, const std::string& name);

}  // namespace pinggen

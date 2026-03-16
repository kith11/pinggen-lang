#pragma once

#include <filesystem>
#include <string>

namespace pinggen {

struct ProjectConfig {
    std::string name;
    std::filesystem::path root;
    std::filesystem::path entry;
    std::filesystem::path output;
};

ProjectConfig load_project(const std::filesystem::path& project_dir);
void create_project(const std::filesystem::path& target_dir, const std::string& name);

}  // namespace pinggen

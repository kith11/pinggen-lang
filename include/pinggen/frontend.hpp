#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "pinggen/ast.hpp"
#include "pinggen/project.hpp"
#include "pinggen/token.hpp"

namespace pinggen {

struct ParsedModule {
    std::filesystem::path path;
    std::string module_name;
    std::vector<Token> tokens;
    Program program;
};

struct FrontendResult {
    ProjectConfig project;
    BuildTarget target;
    Program merged_program;
    std::vector<ParsedModule> modules;
};

std::filesystem::path find_project_root(const std::filesystem::path& source_path);
FrontendResult load_frontend_project(
    const std::filesystem::path& project_dir, const std::optional<std::string>& target_name = std::nullopt,
    const std::unordered_map<std::string, std::string>& source_overrides = {});

}  // namespace pinggen

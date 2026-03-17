#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "pinggen/ast.hpp"

namespace pinggen {

struct FormatterSummary {
    std::vector<std::filesystem::path> visited_files;
    std::vector<std::filesystem::path> changed_files;
};

std::string format_program(const Program& program);
std::string format_source(const std::string& source);
FormatterSummary format_project_sources(const std::filesystem::path& project_dir, bool check_only);

}  // namespace pinggen

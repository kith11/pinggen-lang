#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "pinggen/project.hpp"

namespace pinggen {

struct DependencyStatus {
    std::string name;
    DependencySourceKind source_kind = DependencySourceKind::Path;
    std::optional<std::string> version_requirement;
    std::optional<std::string> resolved_version;
    std::filesystem::path path;
    bool from_lockfile = false;
};

ProjectConfig resolve_registry_dependencies(const ProjectConfig& project,
                                            const std::optional<std::string>& inherited_registry_index = std::nullopt,
                                            bool force_refresh = false);
std::string select_default_registry_requirement(const std::string& registry_index, const std::string& package_name);
std::vector<DependencyStatus> collect_dependency_status(const ProjectConfig& project,
                                                        const std::optional<std::string>& inherited_registry_index = std::nullopt);

}  // namespace pinggen

#pragma once

#include <optional>

#include "pinggen/project.hpp"

namespace pinggen {

ProjectConfig resolve_registry_dependencies(const ProjectConfig& project,
                                            const std::optional<std::string>& inherited_registry_index = std::nullopt);

}  // namespace pinggen

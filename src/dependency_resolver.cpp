#include "pinggen/dependency_resolver.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace pinggen {

namespace {

struct RegistryPackageRecord {
    std::string name;
    std::string version;
    std::string url;
    std::string checksum;
};

struct LockDependencyRecord {
    std::string name;
    std::string package;
    std::string requirement;
    std::string resolved_version;
    std::string registry_index;
    std::string checksum;
    fs::path path;
};

struct Lockfile {
    std::string registry_index;
    std::vector<LockDependencyRecord> dependencies;
};

struct SemVer {
    int major = 0;
    int minor = 0;
    int patch = 0;
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

std::string quote_ps(const std::string& value) {
    std::string escaped;
    escaped.reserve(value.size() + 2);
    for (char ch : value) {
        if (ch == '\'') {
            escaped += "''";
        } else {
            escaped += ch;
        }
    }
    return "'" + escaped + "'";
}

std::string read_file_text(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read file '" + path.string() + "'");
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

#ifdef _WIN32
#define POPEN _popen
#define PCLOSE _pclose
#else
#define POPEN popen
#define PCLOSE pclose
#endif

std::string run_and_capture(const std::string& command) {
    std::array<char, 512> buffer{};
    std::string output;
    FILE* pipe = POPEN(command.c_str(), "r");
    if (!pipe) {
        throw std::runtime_error("failed to run command");
    }
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }
    const int exit_code = PCLOSE(pipe);
    if (exit_code != 0) {
        throw std::runtime_error("command failed: " + command);
    }
    return trim(output);
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::uint64_t fnv1a_hash(const std::string& value) {
    std::uint64_t hash = 1469598103934665603ull;
    for (unsigned char ch : value) {
        hash ^= ch;
        hash *= 1099511628211ull;
    }
    return hash;
}

std::string to_hex(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << value;
    return out.str();
}

int compare_semver(const SemVer& lhs, const SemVer& rhs) {
    if (lhs.major != rhs.major) {
        return lhs.major < rhs.major ? -1 : 1;
    }
    if (lhs.minor != rhs.minor) {
        return lhs.minor < rhs.minor ? -1 : 1;
    }
    if (lhs.patch != rhs.patch) {
        return lhs.patch < rhs.patch ? -1 : 1;
    }
    return 0;
}

SemVer parse_semver(const std::string& text) {
    SemVer version;
    std::stringstream stream(text);
    std::string segment;
    if (!std::getline(stream, segment, '.')) {
        throw std::runtime_error("invalid version requirement '" + text + "'");
    }
    version.major = std::stoi(segment);
    if (!std::getline(stream, segment, '.')) {
        throw std::runtime_error("invalid version requirement '" + text + "'");
    }
    version.minor = std::stoi(segment);
    if (!std::getline(stream, segment, '.')) {
        throw std::runtime_error("invalid version requirement '" + text + "'");
    }
    version.patch = std::stoi(segment);
    if (std::getline(stream, segment, '.')) {
        throw std::runtime_error("invalid version requirement '" + text + "'");
    }
    return version;
}

bool is_valid_version_requirement(const std::string& requirement) {
    try {
        if (starts_with(requirement, "^")) {
            parse_semver(requirement.substr(1));
            return true;
        }
        parse_semver(requirement);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool version_matches_requirement(const std::string& version_text, const std::string& requirement) {
    const SemVer version = parse_semver(version_text);
    if (starts_with(requirement, "^")) {
        const SemVer base = parse_semver(requirement.substr(1));
        if (compare_semver(version, base) < 0) {
            return false;
        }
        SemVer upper = base;
        if (base.major > 0) {
            upper.major += 1;
            upper.minor = 0;
            upper.patch = 0;
        } else if (base.minor > 0) {
            upper.minor += 1;
            upper.patch = 0;
        } else {
            upper.patch += 1;
        }
        return compare_semver(version, upper) < 0;
    }
    return version_text == requirement;
}

fs::path default_cache_root() {
#ifdef _WIN32
    if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
        return fs::path(local_app_data) / "puff" / "cache" / "registry";
    }
#endif
    if (const char* home = std::getenv("USERPROFILE")) {
        return fs::path(home) / ".puff" / "cache" / "registry";
    }
    if (const char* home = std::getenv("HOME")) {
        return fs::path(home) / ".puff" / "cache" / "registry";
    }
    return fs::current_path() / ".puff" / "cache" / "registry";
}

bool is_http_url(const std::string& value) { return starts_with(value, "http://") || starts_with(value, "https://"); }

bool is_file_url(const std::string& value) { return starts_with(value, "file://"); }

fs::path file_url_to_path(const std::string& value) {
    std::string path = value.substr(std::string("file://").size());
    if (starts_with(path, "/") && path.size() > 2 && path[2] == ':') {
        path.erase(path.begin());
    }
    return fs::path(path);
}

std::string normalize_registry_index(const std::string& index, const fs::path& project_root) {
    if (is_http_url(index) || is_file_url(index)) {
        return index;
    }
    const fs::path candidate(index);
    if (candidate.is_absolute()) {
        return candidate.string();
    }
    return (project_root / candidate).lexically_normal().string();
}

fs::path resolve_local_resource_path(const std::string& resource, const std::string& base) {
    if (is_file_url(resource)) {
        return file_url_to_path(resource);
    }
    const fs::path candidate(resource);
    if (candidate.is_absolute()) {
        return candidate;
    }
    if (is_file_url(base)) {
        return file_url_to_path(base).parent_path() / candidate;
    }
    if (!is_http_url(base)) {
        return fs::path(base).parent_path() / candidate;
    }
    throw std::runtime_error("relative registry resource '" + resource + "' requires a local file-based registry index");
}

fs::path fetch_text_resource_to_temp(const std::string& resource, const std::string& base) {
    fs::path temp = fs::temp_directory_path() / ("puff_registry_" + to_hex(fnv1a_hash(resource + base)) + ".tmp");
    if (!is_http_url(resource)) {
        const fs::path local = is_file_url(resource) ? file_url_to_path(resource) : fs::path(resource);
        if (!fs::exists(local)) {
            throw std::runtime_error("registry resource not found at '" + local.string() + "'");
        }
        fs::copy_file(local, temp, fs::copy_options::overwrite_existing);
        return temp;
    }
    const std::string command =
        "powershell -NoProfile -Command \"Invoke-WebRequest -UseBasicParsing -Uri " + quote_ps(resource) +
        " -OutFile " + quote_ps(temp.string()) + "\"";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("failed to download registry resource '" + resource + "'");
    }
    return temp;
}

std::vector<RegistryPackageRecord> load_registry_index_records(const std::string& index) {
    const fs::path temp = fetch_text_resource_to_temp(index, index);
    std::ifstream input(temp);
    if (!input) {
        throw std::runtime_error("failed to read registry index '" + index + "'");
    }

    std::vector<RegistryPackageRecord> packages;
    std::optional<RegistryPackageRecord> current;
    std::string line;
    std::string section;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            if (section == "[package]") {
                if (current.has_value()) {
                    packages.push_back(*current);
                }
                current = RegistryPackageRecord{};
            }
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos || !current.has_value()) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = unquote(trim(line.substr(eq + 1)));
        if (key == "name") {
            current->name = value;
        } else if (key == "version") {
            current->version = value;
        } else if (key == "url") {
            current->url = value;
        } else if (key == "checksum") {
            current->checksum = value;
        }
    }
    if (current.has_value()) {
        packages.push_back(*current);
    }
    return packages;
}

std::optional<Lockfile> load_lockfile(const fs::path& path) {
    if (!fs::exists(path)) {
        return std::nullopt;
    }
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read lockfile '" + path.string() + "'");
    }
    Lockfile lockfile;
    std::optional<LockDependencyRecord> current;
    std::string line;
    std::string section;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            if (section == "[registry_dependency]") {
                if (current.has_value()) {
                    lockfile.dependencies.push_back(*current);
                }
                current = LockDependencyRecord{};
            }
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = unquote(trim(line.substr(eq + 1)));
        if (section.empty() && key == "registry_index") {
            lockfile.registry_index = value;
        } else if (section == "[registry_dependency]" && current.has_value()) {
            if (key == "name") {
                current->name = value;
            } else if (key == "package") {
                current->package = value;
            } else if (key == "requirement") {
                current->requirement = value;
            } else if (key == "resolved_version") {
                current->resolved_version = value;
            } else if (key == "registry_index") {
                current->registry_index = value;
            } else if (key == "checksum") {
                current->checksum = value;
            } else if (key == "path") {
                current->path = value;
            }
        }
    }
    if (current.has_value()) {
        lockfile.dependencies.push_back(*current);
    }
    return lockfile;
}

void write_lockfile(const fs::path& path, const Lockfile& lockfile) {
    std::ofstream output(path);
    if (!output) {
        throw std::runtime_error("failed to write lockfile '" + path.string() + "'");
    }
    output << "registry_index = \"" << lockfile.registry_index << "\"\n\n";
    for (const auto& dependency : lockfile.dependencies) {
        output << "[[registry_dependency]]\n";
        output << "name = \"" << dependency.name << "\"\n";
        output << "package = \"" << dependency.package << "\"\n";
        output << "requirement = \"" << dependency.requirement << "\"\n";
        output << "resolved_version = \"" << dependency.resolved_version << "\"\n";
        output << "registry_index = \"" << dependency.registry_index << "\"\n";
        output << "checksum = \"" << dependency.checksum << "\"\n";
        output << "path = \"" << dependency.path.string() << "\"\n\n";
    }
}

std::string compute_sha256(const fs::path& path) {
    const std::string command =
        "powershell -NoProfile -Command \"(Get-FileHash -Algorithm SHA256 " + quote_ps(path.string()) +
        ").Hash.ToLowerInvariant()\"";
    return trim(run_and_capture(command));
}

void download_resource(const std::string& resource, const std::string& base, const fs::path& destination) {
    fs::create_directories(destination.parent_path());
    if (!is_http_url(resource)) {
        const fs::path local = resolve_local_resource_path(resource, base);
        if (!fs::exists(local)) {
            throw std::runtime_error("registry package archive not found at '" + local.string() + "'");
        }
        fs::copy_file(local, destination, fs::copy_options::overwrite_existing);
        return;
    }
    const std::string command =
        "powershell -NoProfile -Command \"Invoke-WebRequest -UseBasicParsing -Uri " + quote_ps(resource) +
        " -OutFile " + quote_ps(destination.string()) + "\"";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("failed to download package archive '" + resource + "'");
    }
}

void unpack_archive(const fs::path& archive, const fs::path& destination) {
    if (fs::exists(destination)) {
        fs::remove_all(destination);
    }
    fs::create_directories(destination);
    const std::string command =
        "powershell -NoProfile -Command \"Expand-Archive -Path " + quote_ps(archive.string()) + " -DestinationPath " +
        quote_ps(destination.string()) + " -Force\"";
    if (std::system(command.c_str()) != 0) {
        throw std::runtime_error("failed to unpack package archive '" + archive.string() + "'");
    }
}

bool lockfile_matches_project(const Lockfile& lockfile, const ProjectConfig& project, const std::string& registry_index) {
    if (lockfile.registry_index != registry_index) {
        return false;
    }
    std::size_t registry_dependency_count = 0;
    for (const auto& dependency : project.dependencies) {
        if (dependency.source_kind != DependencySourceKind::Registry) {
            continue;
        }
        ++registry_dependency_count;
        const auto match_it =
            std::find_if(lockfile.dependencies.begin(), lockfile.dependencies.end(), [&](const LockDependencyRecord& record) {
                return record.name == dependency.name && record.requirement == *dependency.version_requirement &&
                       record.package == dependency.name && record.registry_index == registry_index &&
                       fs::exists(record.path / "pinggen.toml");
            });
        if (match_it == lockfile.dependencies.end()) {
            return false;
        }
    }
    return registry_dependency_count == lockfile.dependencies.size();
}

const RegistryPackageRecord& find_registry_package(const std::vector<RegistryPackageRecord>& packages,
                                                   const std::string& name,
                                                   const std::string& requirement,
                                                   const std::string& index) {
    if (!is_valid_version_requirement(requirement)) {
        throw std::runtime_error("invalid version requirement '" + requirement + "'");
    }
    std::vector<const RegistryPackageRecord*> candidates;
    for (const auto& record : packages) {
        if (record.name == name && version_matches_requirement(record.version, requirement)) {
            candidates.push_back(&record);
        }
    }
    if (candidates.empty()) {
        const bool package_exists =
            std::any_of(packages.begin(), packages.end(), [&](const RegistryPackageRecord& record) { return record.name == name; });
        if (!package_exists) {
            throw std::runtime_error("unknown registry package '" + name + "' in registry '" + index + "'");
        }
        throw std::runtime_error("registry package '" + name + "' does not provide a version matching '" + requirement + "'");
    }
    const auto best = std::max_element(candidates.begin(), candidates.end(), [](const RegistryPackageRecord* lhs,
                                                                                const RegistryPackageRecord* rhs) {
        return compare_semver(parse_semver(lhs->version), parse_semver(rhs->version)) < 0;
    });
    return **best;
}

}  // namespace

ProjectConfig resolve_registry_dependencies(const ProjectConfig& project,
                                            const std::optional<std::string>& inherited_registry_index,
                                            bool force_refresh) {
    ProjectConfig resolved = project;
    const bool has_registry_dependencies =
        std::any_of(resolved.dependencies.begin(), resolved.dependencies.end(), [](const DependencyConfig& dependency) {
            return dependency.source_kind == DependencySourceKind::Registry;
        });
    if (!has_registry_dependencies) {
        return resolved;
    }

    if (resolved.registry.index.has_value()) {
        resolved.registry.index = normalize_registry_index(*resolved.registry.index, resolved.root);
    }
    const std::optional<std::string> effective_registry =
        resolved.registry.index.has_value() ? resolved.registry.index : inherited_registry_index;
    if (!effective_registry.has_value()) {
        throw std::runtime_error("project '" + resolved.name + "' requires registry dependencies but no registry index is configured");
    }

    const bool use_lockfile = !inherited_registry_index.has_value();
    std::optional<Lockfile> lockfile;
    const fs::path lockfile_path = resolved.root / "puff.lock";
    if (use_lockfile) {
        lockfile = load_lockfile(lockfile_path);
    }
    const bool lockfile_valid =
        !force_refresh && lockfile.has_value() && lockfile_matches_project(*lockfile, resolved, *effective_registry);
    if (use_lockfile && !force_refresh && lockfile.has_value() && !lockfile_valid) {
        throw std::runtime_error("lockfile '" + lockfile_path.string() +
                                 "' is stale for the current manifest; run 'puff update' to refresh dependencies");
    }

    std::vector<RegistryPackageRecord> registry_packages;
    bool registry_loaded = false;
    Lockfile new_lockfile;
    new_lockfile.registry_index = *effective_registry;

    for (auto& dependency : resolved.dependencies) {
        if (dependency.source_kind == DependencySourceKind::Path) {
            dependency.resolved_path = *dependency.path;
            continue;
        }

        if (lockfile_valid) {
            const auto record_it =
                std::find_if(lockfile->dependencies.begin(), lockfile->dependencies.end(),
                             [&](const LockDependencyRecord& record) { return record.name == dependency.name; });
            dependency.resolved_path = record_it->path;
            dependency.resolved_version = record_it->resolved_version;
            continue;
        }

        if (!registry_loaded) {
            registry_packages = load_registry_index_records(*effective_registry);
            registry_loaded = true;
        }

        const RegistryPackageRecord& record =
            find_registry_package(registry_packages, dependency.name, *dependency.version_requirement, *effective_registry);
        const fs::path cache_base =
            default_cache_root() / to_hex(fnv1a_hash(*effective_registry)) / dependency.name / record.version;
        const fs::path archive_path = cache_base / "package.zip";
        const fs::path unpacked_path = cache_base / "package";
        if (!fs::exists(archive_path) || compute_sha256(archive_path) != record.checksum) {
            download_resource(record.url, *effective_registry, archive_path);
        }
        const std::string actual_checksum = compute_sha256(archive_path);
        if (actual_checksum != record.checksum) {
            throw std::runtime_error("checksum mismatch for registry package '" + dependency.name + "' version '" +
                                     record.version + "'");
        }
        if (!fs::exists(unpacked_path / "pinggen.toml")) {
            unpack_archive(archive_path, unpacked_path);
        }
        if (!fs::exists(unpacked_path / "pinggen.toml")) {
            throw std::runtime_error("unpacked registry package '" + dependency.name + "' is missing pinggen.toml");
        }

        dependency.resolved_path = unpacked_path;
        dependency.resolved_version = record.version;
        new_lockfile.dependencies.push_back(
            {dependency.name, dependency.name, *dependency.version_requirement, record.version, *effective_registry,
             actual_checksum, unpacked_path});
    }

    if (use_lockfile && (!lockfile_valid || force_refresh || !lockfile.has_value())) {
        write_lockfile(lockfile_path, new_lockfile);
    }

    return resolved;
}

std::string select_default_registry_requirement(const std::string& registry_index, const std::string& package_name) {
    const std::vector<RegistryPackageRecord> packages = load_registry_index_records(registry_index);
    std::vector<const RegistryPackageRecord*> candidates;
    for (const auto& record : packages) {
        if (record.name == package_name) {
            candidates.push_back(&record);
        }
    }
    if (candidates.empty()) {
        throw std::runtime_error("unknown registry package '" + package_name + "' in registry '" + registry_index + "'");
    }
    const auto best = std::max_element(candidates.begin(), candidates.end(), [](const RegistryPackageRecord* lhs,
                                                                                const RegistryPackageRecord* rhs) {
        return compare_semver(parse_semver(lhs->version), parse_semver(rhs->version)) < 0;
    });
    return "^" + (*best)->version;
}

std::vector<DependencyStatus> collect_dependency_status(const ProjectConfig& project,
                                                        const std::optional<std::string>& inherited_registry_index) {
    const ProjectConfig resolved = resolve_registry_dependencies(project, inherited_registry_index);
    std::vector<DependencyStatus> statuses;
    statuses.reserve(resolved.dependencies.size());
    const fs::path lockfile_path = resolved.root / "puff.lock";
    const std::optional<Lockfile> lockfile = load_lockfile(lockfile_path);
    for (const auto& dependency : resolved.dependencies) {
        DependencyStatus status;
        status.name = dependency.name;
        status.source_kind = dependency.source_kind;
        status.version_requirement = dependency.version_requirement;
        status.resolved_version = dependency.resolved_version;
        status.path = dependency.source_kind == DependencySourceKind::Path ? *dependency.path : dependency.resolved_path;
        if (lockfile.has_value() && dependency.source_kind == DependencySourceKind::Registry) {
            status.from_lockfile = std::any_of(lockfile->dependencies.begin(), lockfile->dependencies.end(),
                                               [&](const LockDependencyRecord& record) { return record.name == dependency.name; });
        }
        statuses.push_back(std::move(status));
    }
    return statuses;
}

}  // namespace pinggen

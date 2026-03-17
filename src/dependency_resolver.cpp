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
    std::string version;
    std::string registry_index;
    std::string checksum;
    fs::path path;
};

struct Lockfile {
    std::string registry_index;
    std::vector<LockDependencyRecord> dependencies;
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
            } else if (key == "version") {
                current->version = value;
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
        output << "version = \"" << dependency.version << "\"\n";
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
                return record.name == dependency.name && record.version == *dependency.version &&
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
                                                   const std::string& version,
                                                   const std::string& index) {
    const auto it = std::find_if(packages.begin(), packages.end(), [&](const RegistryPackageRecord& record) {
        return record.name == name && record.version == version;
    });
    if (it == packages.end()) {
        const bool package_exists =
            std::any_of(packages.begin(), packages.end(), [&](const RegistryPackageRecord& record) { return record.name == name; });
        if (!package_exists) {
            throw std::runtime_error("unknown registry package '" + name + "' in registry '" + index + "'");
        }
        throw std::runtime_error("registry package '" + name + "' does not provide exact version '" + version + "'");
    }
    return *it;
}

}  // namespace

ProjectConfig resolve_registry_dependencies(const ProjectConfig& project,
                                            const std::optional<std::string>& inherited_registry_index) {
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
    const bool lockfile_valid = lockfile.has_value() && lockfile_matches_project(*lockfile, resolved, *effective_registry);

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
            continue;
        }

        if (!registry_loaded) {
            registry_packages = load_registry_index_records(*effective_registry);
            registry_loaded = true;
        }

        const RegistryPackageRecord& record =
            find_registry_package(registry_packages, dependency.name, *dependency.version, *effective_registry);
        const fs::path cache_base =
            default_cache_root() / to_hex(fnv1a_hash(*effective_registry)) / dependency.name / *dependency.version;
        const fs::path archive_path = cache_base / "package.zip";
        const fs::path unpacked_path = cache_base / "package";
        if (!fs::exists(archive_path) || compute_sha256(archive_path) != record.checksum) {
            download_resource(record.url, *effective_registry, archive_path);
        }
        const std::string actual_checksum = compute_sha256(archive_path);
        if (actual_checksum != record.checksum) {
            throw std::runtime_error("checksum mismatch for registry package '" + dependency.name + "' version '" +
                                     *dependency.version + "'");
        }
        if (!fs::exists(unpacked_path / "pinggen.toml")) {
            unpack_archive(archive_path, unpacked_path);
        }
        if (!fs::exists(unpacked_path / "pinggen.toml")) {
            throw std::runtime_error("unpacked registry package '" + dependency.name + "' is missing pinggen.toml");
        }

        dependency.resolved_path = unpacked_path;
        new_lockfile.dependencies.push_back(
            {dependency.name, dependency.name, *dependency.version, *effective_registry, actual_checksum, unpacked_path});
    }

    if (use_lockfile && !lockfile_valid) {
        write_lockfile(lockfile_path, new_lockfile);
    }

    return resolved;
}

}  // namespace pinggen

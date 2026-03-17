#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "pinggen/diagnostics.hpp"
#include "pinggen/dependency_resolver.hpp"
#include "pinggen/formatter.hpp"
#include "pinggen/frontend.hpp"
#include "pinggen/lsp.hpp"
#include "pinggen/llvm_ir.hpp"
#include "pinggen/project.hpp"
#include "pinggen/sema.hpp"

namespace fs = std::filesystem;

namespace pinggen {

namespace {

struct PackageSpec {
    std::string name;
    std::optional<std::string> version_requirement;
};

std::string trim_copy(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.rfind(prefix, 0) == 0;
}

std::string prompt_line(const std::string& prompt, const std::string& fallback = "") {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    line = trim_copy(line);
    if (line.empty()) {
        return fallback;
    }
    return line;
}

PackageSpec parse_package_spec(const std::string& spec) {
    const std::size_t at = spec.find('@');
    PackageSpec parsed;
    parsed.name = at == std::string::npos ? spec : spec.substr(0, at);
    if (parsed.name.empty()) {
        throw std::runtime_error("package name cannot be empty");
    }
    if (at != std::string::npos) {
        parsed.version_requirement = spec.substr(at + 1);
        if (parsed.version_requirement->empty()) {
            throw std::runtime_error("package version requirement cannot be empty");
        }
    }
    return parsed;
}

bool prompt_yes_no(const std::string& prompt, bool default_value) {
    const std::string suffix = default_value ? " [Y/n]: " : " [y/N]: ";
    const std::string answer = prompt_line(prompt + suffix);
    if (answer.empty()) {
        return default_value;
    }
    if (answer == "y" || answer == "Y" || answer == "yes" || answer == "YES") {
        return true;
    }
    if (answer == "n" || answer == "N" || answer == "no" || answer == "NO") {
        return false;
    }
    return default_value;
}

std::string null_redirect() {
#ifdef _WIN32
    return " >nul 2>nul";
#else
    return " >/dev/null 2>/dev/null";
#endif
}

bool command_available(const std::string& command) {
    return std::system((command + null_redirect()).c_str()) == 0;
}

std::string command_name_from_path(const std::filesystem::path& executable_path) {
    const std::string stem = executable_path.stem().string();
    if (!stem.empty()) {
        return stem;
    }
    return "puff";
}

std::filesystem::path default_install_bin_dir() {
#ifdef _WIN32
    if (const char* local_app_data = std::getenv("LOCALAPPDATA")) {
        return std::filesystem::path(local_app_data) / "puff" / "bin";
    }
#endif
    if (const char* home = std::getenv("USERPROFILE")) {
        return std::filesystem::path(home) / ".puff" / "bin";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".puff" / "bin";
    }
    return std::filesystem::current_path() / ".puff" / "bin";
}

std::filesystem::path install_binary(const std::filesystem::path& executable_path, const std::filesystem::path& bin_dir,
                                     const std::string& command_name) {
    std::filesystem::create_directories(bin_dir);
    const std::filesystem::path destination = bin_dir / (command_name + ".exe");
    std::filesystem::copy_file(executable_path, destination, std::filesystem::copy_options::overwrite_existing);
    return destination;
}

std::filesystem::path write_windows_launcher(const std::filesystem::path& bin_dir, const std::string& command_name) {
    const std::filesystem::path launcher_path = bin_dir / (command_name + ".cmd");
    std::ofstream launcher(launcher_path);
    launcher << "@echo off\n";
    launcher << "set SCRIPT_DIR=%~dp0\n";
    launcher << "\"%SCRIPT_DIR%" << command_name << ".exe\" %*\n";
    return launcher_path;
}

void print_usage(std::ostream& out, const std::string& command_name) {
    out << command_name << " commands:\n";
    out << "  " << command_name << " help\n";
    out << "  " << command_name << " new <path>\n";
    out << "  " << command_name << " init <path>\n";
    out << "  " << command_name << " check [path]\n";
    out << "  " << command_name << " fmt [path] [--check]\n";
    out << "  " << command_name << " lsp\n";
    out << "  " << command_name << " build [path] [--target <name>]\n";
    out << "  " << command_name << " run [path] [--target <name>]\n";
    out << "  " << command_name << " add <name>[@version] [path]\n";
    out << "  " << command_name << " update [name] [path]\n";
    out << "  " << command_name << " deps [path]\n";
    out << "  " << command_name << " targets [path]\n";
    out << "  " << command_name << " doctor [path]\n";
    out << "  " << command_name << " install [--bin-dir <path>]\n";
    out << "  " << command_name << " setup [project-path] [--bin-dir <path>]\n\n";
    out << "start here:\n";
    out << "  " << command_name << " doctor         check toolchain and current project\n";
    out << "  " << command_name << " init my_app     create a starter project\n";
    out << "  " << command_name << " fmt my_app      format project source files\n";
    out << "  " << command_name << " lsp             start the language server\n";
    out << "  " << command_name << " run my_app      build and run the default target\n";
}

std::string choose_version_requirement(const ProjectConfig& project, const PackageSpec& spec) {
    if (spec.version_requirement.has_value()) {
        return *spec.version_requirement;
    }
    if (!project.registry.index.has_value()) {
        throw std::runtime_error("project '" + project.name + "' must define [registry].index before using 'puff add'");
    }
    return select_default_registry_requirement(*project.registry.index, spec.name);
}

void rewrite_dependency_requirement(const fs::path& project_dir, const std::string& dependency_name,
                                   const std::string& new_requirement) {
    const fs::path config_path = project_dir / "pinggen.toml";
    std::ifstream input(config_path);
    if (!input) {
        throw std::runtime_error("missing project config '" + config_path.string() + "'");
    }

    std::vector<std::string> lines;
    std::string line;
    while (std::getline(input, line)) {
        lines.push_back(line);
    }

    bool in_dependency = false;
    bool matches_dependency = false;
    bool updated = false;
    for (auto& current_line : lines) {
        const std::string trimmed = trim_copy(current_line);
        if (trimmed == "[[dependency]]") {
            in_dependency = true;
            matches_dependency = false;
            continue;
        }
        if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
            in_dependency = false;
            matches_dependency = false;
            continue;
        }
        if (!in_dependency) {
            continue;
        }
        if (starts_with(trimmed, "name = ")) {
            matches_dependency = trimmed == "name = \"" + dependency_name + "\"";
            continue;
        }
        if (matches_dependency && starts_with(trimmed, "version = ")) {
            current_line = "version = \"" + new_requirement + "\"";
            updated = true;
            break;
        }
    }

    if (!updated) {
        throw std::runtime_error("project '" + project_dir.filename().string() + "' does not define registry dependency '" +
                                 dependency_name + "'");
    }

    std::ofstream output(config_path, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("failed to update project config '" + config_path.string() + "'");
    }
    for (std::size_t i = 0; i < lines.size(); ++i) {
        output << lines[i];
        if (i + 1 < lines.size()) {
            output << '\n';
        }
    }
}

void print_targets(const ProjectConfig& project) {
    std::cout << "project " << project.name << '\n';
    std::cout << "default target: " << project.default_target.name << '\n';
    std::cout << "  entry: " << project.default_target.entry.string() << '\n';
    std::cout << "  output: " << project.default_target.output.string() << '\n';
    if (project.extra_targets.empty()) {
        std::cout << "named targets: none\n";
        return;
    }
    std::cout << "named targets:\n";
    for (const auto& target : project.extra_targets) {
        std::cout << "  - " << target.name << '\n';
        std::cout << "    entry: " << target.entry.string() << '\n';
        std::cout << "    output: " << target.output.string() << '\n';
    }
}

int command_targets(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    print_targets(project);
    return 0;
}

int command_deps(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    const std::vector<DependencyStatus> statuses = collect_dependency_status(project);
    std::cout << "project " << project.name << '\n';
    if (statuses.empty()) {
        std::cout << "dependencies: none\n";
        return 0;
    }
    std::cout << "dependencies:\n";
    for (const auto& status : statuses) {
        std::cout << "  - " << status.name << '\n';
        if (status.source_kind == DependencySourceKind::Path) {
            std::cout << "    source: path\n";
            std::cout << "    path: " << status.path.string() << '\n';
        } else {
            std::cout << "    source: registry\n";
            std::cout << "    requirement: " << *status.version_requirement << '\n';
            std::cout << "    resolved: " << *status.resolved_version << '\n';
            std::cout << "    cache: " << status.path.string() << '\n';
            std::cout << "    lockfile: " << (status.from_lockfile ? "present" : "generated") << '\n';
        }
    }
    return 0;
}

int command_doctor(const fs::path& project_dir, const fs::path& executable_path, const std::string& command_name) {
    const bool has_clang = command_available("clang --version");
    const bool has_clangxx = command_available("clang++ --version");

    std::cout << command_name << " doctor\n";
    std::cout << "binary: " << executable_path.string() << '\n';
    std::cout << "clang: " << (has_clang ? "found" : "missing") << '\n';
    std::cout << "clang++: " << (has_clangxx ? "found" : "missing") << '\n';

    const fs::path config_path = project_dir / "pinggen.toml";
    if (fs::exists(config_path)) {
        const ProjectConfig project = load_project(project_dir);
        print_targets(project);
    } else {
        std::cout << "project: no pinggen.toml found at " << config_path.string() << '\n';
    }

    if (!has_clang || !has_clangxx) {
        std::cout << "toolchain status: incomplete\n";
        return 1;
    }
    std::cout << "toolchain status: ready\n";
    return 0;
}

int command_add(const fs::path& project_dir, const std::string& package_spec_text) {
    const ProjectConfig project = load_project(project_dir);
    const PackageSpec package_spec = parse_package_spec(package_spec_text);
    const std::string version_requirement = choose_version_requirement(project, package_spec);
    const fs::path manifest_path = project.root / "pinggen.toml";
    std::ifstream input(manifest_path);
    std::ostringstream original_manifest;
    original_manifest << input.rdbuf();

    add_registry_dependency(project_dir, package_spec.name, version_requirement);
    try {
        const ProjectConfig updated_project = load_project(project_dir);
        resolve_registry_dependencies(updated_project, std::nullopt, true);
    } catch (...) {
        std::ofstream restore(manifest_path, std::ios::trunc);
        restore << original_manifest.str();
        throw;
    }
    std::cout << "added " << package_spec.name << " " << version_requirement << '\n';
    return 0;
}

int command_update(const fs::path& project_dir, const std::optional<std::string>& dependency_name) {
    const ProjectConfig project = load_project(project_dir);
    if (dependency_name.has_value()) {
        const auto it = std::find_if(project.dependencies.begin(), project.dependencies.end(), [&](const DependencyConfig& dependency) {
            return dependency.name == *dependency_name && dependency.source_kind == DependencySourceKind::Registry;
        });
        if (it == project.dependencies.end()) {
            throw std::runtime_error("project '" + project.name + "' does not define registry dependency '" + *dependency_name + "'");
        }
    }
    const ProjectConfig updated = resolve_registry_dependencies(project, std::nullopt, true);
    if (dependency_name.has_value()) {
        const auto it = std::find_if(updated.dependencies.begin(), updated.dependencies.end(), [&](const DependencyConfig& dependency) {
            return dependency.name == *dependency_name;
        });
        std::cout << "updated " << *dependency_name << " -> " << *it->resolved_version << '\n';
    } else {
        std::cout << "updated " << project.name << " dependencies\n";
    }
    return 0;
}

int command_install(const fs::path& executable_path, const std::optional<fs::path>& bin_dir_override,
                    const std::string& command_name) {
    const fs::path bin_dir = bin_dir_override.value_or(default_install_bin_dir());
    const fs::path installed_path = install_binary(executable_path, bin_dir, command_name);
    const fs::path launcher_path = write_windows_launcher(bin_dir, command_name);
    std::cout << "installed " << installed_path.string() << '\n';
    std::cout << "launcher " << launcher_path.string() << '\n';
    std::cout << "add this directory to PATH if you want to run " << command_name << " globally:\n";
    std::cout << "  " << bin_dir.string() << '\n';
    return 0;
}

int command_setup(const fs::path& project_dir, const fs::path& executable_path, const std::optional<fs::path>& bin_dir_override,
                  const std::string& command_name) {
    const fs::path default_project_dir = project_dir.empty() ? (fs::current_path() / "puff_app") : project_dir;
    const fs::path default_bin_dir = bin_dir_override.value_or(default_install_bin_dir());

    std::cout << command_name << " setup\n";
    std::cout << "This wizard can install the compiler binary and create a starter project.\n\n";
    command_doctor(fs::current_path(), executable_path, command_name);
    std::cout << '\n';

    if (prompt_yes_no("Install " + command_name + " to " + default_bin_dir.string() + "?", true)) {
        const fs::path installed_path = install_binary(executable_path, default_bin_dir, command_name);
        const fs::path launcher_path = write_windows_launcher(default_bin_dir, command_name);
        std::cout << "installed " << installed_path.string() << "\n\n";
        std::cout << "launcher " << launcher_path.string() << "\n\n";
    }

    if (prompt_yes_no("Create a starter project?", true)) {
        const std::string chosen_path =
            prompt_line("Project directory [" + default_project_dir.string() + "]: ", default_project_dir.string());
        const fs::path chosen_dir = chosen_path;
        create_project(chosen_dir, chosen_dir.filename().string());
        std::cout << "created " << chosen_dir.string() << '\n';
        std::cout << "next:\n";
        std::cout << "  " << command_name << " run " << chosen_dir.string() << '\n';
    }

    return 0;
}

}  // namespace

static int command_check(const fs::path& project_dir) {
    const FrontendResult frontend = load_frontend_project(project_dir);
    std::cout << "checked " << frontend.project.name << '\n';
    return 0;
}

static int command_fmt(const fs::path& project_dir, bool check_only) {
    const ProjectConfig project = load_project(project_dir);
    const FormatterSummary summary = format_project_sources(project_dir, check_only);
    if (check_only) {
        if (summary.changed_files.empty()) {
            std::cout << "formatted " << project.name << '\n';
            return 0;
        }
        std::cout << "would reformat " << summary.changed_files.size() << " file(s) in " << project.name << ":\n";
        for (const auto& path : summary.changed_files) {
            std::cout << "  " << path.string() << '\n';
        }
        return 1;
    }
    if (summary.changed_files.empty()) {
        std::cout << "formatted " << project.name << " (no changes)\n";
        return 0;
    }
    std::cout << "formatted " << project.name << " (" << summary.changed_files.size() << " file(s) updated)\n";
    return 0;
}

static int command_build(const fs::path& project_dir, const std::optional<std::string>& target_name) {
    const FrontendResult frontend = load_frontend_project(project_dir, target_name);
    LLVMIRGenerator ir;
    const std::string llvm = ir.generate(frontend.merged_program);

    fs::create_directories(frontend.target.output.parent_path());
    const std::string ll_path = frontend.target.output.string() + ".ll";
    const std::string obj_path = frontend.target.output.string() + ".obj";
    const std::string runtime_obj_path = frontend.target.output.string() + ".runtime.obj";
    const std::string exe_path = frontend.target.output.string() + ".exe";
    const fs::path runtime_source = fs::path(PINGGEN_SOURCE_ROOT) / "runtime" / "pinggen_runtime.cpp";

    {
        std::ofstream ll_output(ll_path);
        ll_output << llvm;
    }

    const std::string compile_command = "clang -c \"" + ll_path + "\" -o \"" + obj_path + "\"";
    if (std::system(compile_command.c_str()) != 0) {
        throw std::runtime_error("clang failed while compiling target '" + frontend.target.name + "'");
    }

    const std::string runtime_compile_command =
        "clang++ -c \"" + runtime_source.string() + "\" -o \"" + runtime_obj_path + "\"";
    if (std::system(runtime_compile_command.c_str()) != 0) {
        throw std::runtime_error("clang++ failed while compiling pinggen runtime");
    }

    const std::string link_command =
        "clang++ \"" + obj_path + "\" \"" + runtime_obj_path + "\" -o \"" + exe_path + "\"";
    if (std::system(link_command.c_str()) != 0) {
        throw std::runtime_error("clang failed while linking target '" + frontend.target.name + "'");
    }

    std::cout << "built " << exe_path << '\n';
    return 0;
}

static int command_run(const fs::path& project_dir, const std::optional<std::string>& target_name) {
    const ProjectConfig project = load_project(project_dir);
    const BuildTarget& target = resolve_target(project, target_name);
    command_build(project_dir, target.name);
    const fs::path previous_dir = fs::current_path();
    fs::current_path(project.root);
    const std::string command = "\"" + target.output.string() + ".exe\"";
    const int exit_code = std::system(command.c_str());
    fs::current_path(previous_dir);
    return exit_code;
}

static int command_new(const fs::path& target_dir) {
    create_project(target_dir, target_dir.filename().string());
    std::cout << "created " << target_dir.string() << '\n';
    return 0;
}

}  // namespace pinggen

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            pinggen::print_usage(std::cout, "puff");
            return 0;
        }

        const std::string command = argv[1];
        const fs::path executable_path = fs::absolute(fs::path(argv[0]));
        const std::string command_name = pinggen::command_name_from_path(executable_path);

        if (command == "add") {
            if (argc < 3 || argc > 4) {
                throw std::runtime_error("usage: " + command_name + " add <name>[@version] [path]");
            }
            const fs::path project_dir = argc == 4 ? fs::path(argv[3]) : fs::current_path();
            return pinggen::command_add(project_dir, argv[2]);
        }
        if (command == "update") {
            if (argc > 4) {
                throw std::runtime_error("usage: " + command_name + " update [name] [path]");
            }
            std::optional<std::string> dependency_name;
            fs::path project_dir = fs::current_path();
            if (argc >= 3) {
                const std::string first = argv[2];
                if (first.find('\\') != std::string::npos || first.find('/') != std::string::npos || fs::exists(first)) {
                    project_dir = fs::path(first);
                } else {
                    dependency_name = first;
                }
            }
            if (argc == 4) {
                project_dir = fs::path(argv[3]);
            }
            return pinggen::command_update(project_dir, dependency_name);
        }
        if (command == "deps") {
            if (argc > 3) {
                throw std::runtime_error("usage: " + command_name + " deps [path]");
            }
            const fs::path project_dir = argc == 3 ? fs::path(argv[2]) : fs::current_path();
            return pinggen::command_deps(project_dir);
        }

        fs::path path = fs::current_path();
        std::optional<std::string> target_name;
        std::optional<fs::path> bin_dir;
        bool check_only = false;
        bool path_set = false;
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--target") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("missing target name after --target");
                }
                target_name = argv[++i];
                continue;
            }
            if (arg == "--bin-dir") {
                if (i + 1 >= argc) {
                    throw std::runtime_error("missing path after --bin-dir");
                }
                bin_dir = fs::path(argv[++i]);
                continue;
            }
            if (arg == "--check") {
                check_only = true;
                continue;
            }
            if (path_set) {
                throw std::runtime_error("unexpected argument: " + arg);
            }
            path = fs::path(arg);
            path_set = true;
        }

        if (command == "help" || command == "--help" || command == "-h") {
            pinggen::print_usage(std::cout, command_name);
            return 0;
        }
        if (command == "lsp") {
            if (argc != 2) {
                throw std::runtime_error("usage: " + command_name + " lsp");
            }
            return pinggen::command_lsp();
        }
        if (command == "new") {
            if (target_name.has_value() || bin_dir.has_value()) {
                throw std::runtime_error("new does not support --target or --bin-dir");
            }
            return pinggen::command_new(path);
        }
        if (command == "init") {
            if (target_name.has_value() || bin_dir.has_value()) {
                throw std::runtime_error("init does not support --target or --bin-dir");
            }
            return pinggen::command_new(path);
        }
        if (command == "check") {
            if (target_name.has_value() || bin_dir.has_value() || check_only) {
                throw std::runtime_error("check does not support --target, --bin-dir, or --check");
            }
            return pinggen::command_check(path);
        }
        if (command == "fmt") {
            if (target_name.has_value() || bin_dir.has_value()) {
                throw std::runtime_error("fmt does not support --target or --bin-dir");
            }
            return pinggen::command_fmt(path, check_only);
        }
        if (command == "build") {
            if (bin_dir.has_value() || check_only) {
                throw std::runtime_error("build does not support --bin-dir or --check");
            }
            return pinggen::command_build(path, target_name);
        }
        if (command == "run") {
            if (bin_dir.has_value() || check_only) {
                throw std::runtime_error("run does not support --bin-dir or --check");
            }
            return pinggen::command_run(path, target_name);
        }
        if (command == "targets") {
            if (target_name.has_value() || bin_dir.has_value() || check_only) {
                throw std::runtime_error("targets does not support --target, --bin-dir, or --check");
            }
            return pinggen::command_targets(path);
        }
        if (command == "doctor") {
            if (target_name.has_value() || bin_dir.has_value() || check_only) {
                throw std::runtime_error("doctor does not support --target, --bin-dir, or --check");
            }
            return pinggen::command_doctor(path, executable_path, command_name);
        }
        if (command == "install") {
            if (target_name.has_value() || check_only) {
                throw std::runtime_error("install does not support --target or --check");
            }
            return pinggen::command_install(executable_path, bin_dir, command_name);
        }
        if (command == "setup") {
            if (target_name.has_value() || check_only) {
                throw std::runtime_error("setup does not support --target or --check");
            }
            return pinggen::command_setup(path_set ? path : fs::path(), executable_path, bin_dir, command_name);
        }

        std::cerr << "unknown command: " << command << '\n';
        pinggen::print_usage(std::cerr, command_name);
        return 1;
    } catch (const pinggen::CompileError& error) {
        std::cerr << "compile error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

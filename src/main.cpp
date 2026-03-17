#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "pinggen/diagnostics.hpp"
#include "pinggen/lexer.hpp"
#include "pinggen/llvm_ir.hpp"
#include "pinggen/parser.hpp"
#include "pinggen/project.hpp"
#include "pinggen/sema.hpp"

namespace fs = std::filesystem;

namespace pinggen {

namespace {

std::string trim_copy(const std::string& value) {
    const std::size_t start = value.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return "";
    }
    const std::size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
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
    out << "  " << command_name << " build [path] [--target <name>]\n";
    out << "  " << command_name << " run [path] [--target <name>]\n";
    out << "  " << command_name << " targets [path]\n";
    out << "  " << command_name << " doctor [path]\n";
    out << "  " << command_name << " install [--bin-dir <path>]\n";
    out << "  " << command_name << " setup [project-path] [--bin-dir <path>]\n\n";
    out << "start here:\n";
    out << "  " << command_name << " doctor         check toolchain and current project\n";
    out << "  " << command_name << " init my_app     create a starter project\n";
    out << "  " << command_name << " run my_app      build and run the default target\n";
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

static std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to read source file '" + path.string() + "'");
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
}

static std::vector<std::string> split_module_name(const std::string& module_name) {
    std::vector<std::string> segments;
    std::size_t start = 0;
    while (start < module_name.size()) {
        const std::size_t separator = module_name.find("::", start);
        if (separator == std::string::npos) {
            segments.push_back(module_name.substr(start));
            break;
        }
        segments.push_back(module_name.substr(start, separator - start));
        start = separator + 2;
    }
    return segments;
}

static fs::path project_module_path(const ProjectConfig& project, const std::string& module_name) {
    fs::path path = project.root / "src";
    for (const auto& segment : split_module_name(module_name)) {
        path /= segment;
    }
    path += ".pg";
    return path;
}

static std::string module_name_from_source_path(const ProjectConfig& project, const fs::path& path) {
    const fs::path source_root = project.root / "src";
    std::error_code error;
    fs::path relative = fs::relative(path, source_root, error);
    if (error || relative.empty()) {
        return path.stem().string();
    }
    relative.replace_extension();
    std::string module_name;
    for (const auto& segment : relative) {
        if (!module_name.empty()) {
            module_name += "::";
        }
        module_name += segment.string();
    }
    if (module_name.empty()) {
        return path.stem().string();
    }
    return module_name;
}

static Program parse_file(const fs::path& path) {
    Lexer lexer(read_file(path));
    Parser parser(lexer.tokenize());
    return parser.parse();
}

static void append_program(Program& target, Program source) {
    for (auto& import_decl : source.imports) {
        target.imports.push_back(std::move(import_decl));
    }
    for (auto& enum_decl : source.enums) {
        target.enums.push_back(std::move(enum_decl));
    }
    for (auto& struct_decl : source.structs) {
        target.structs.push_back(std::move(struct_decl));
    }
    for (auto& function_decl : source.functions) {
        target.functions.push_back(std::move(function_decl));
    }
}

static void load_module_graph(const ProjectConfig& project, const fs::path& path, const std::string& module_name, Program& merged,
                              std::unordered_set<std::string>& loaded_modules,
                              std::unordered_set<std::string>& active_modules) {
    Program program = parse_file(path);
    active_modules.insert(module_name);
    for (const auto& import_decl : program.imports) {
        if (import_decl.kind != ImportKind::Module) {
            continue;
        }
        const std::string& imported_name = import_decl.module_name;
        if (active_modules.contains(imported_name)) {
            fail(import_decl.location, "circular module import detected for '" + imported_name + "'");
        }
        if (loaded_modules.contains(imported_name)) {
            continue;
        }
        const fs::path module_path = project_module_path(project, imported_name);
        if (!fs::exists(module_path)) {
            fail(import_decl.location,
                 "missing imported module '" + imported_name + "'; expected file '" + module_path.string() + "'");
        }
        loaded_modules.insert(imported_name);
        load_module_graph(project, module_path, imported_name, merged, loaded_modules, active_modules);
    }
    active_modules.erase(module_name);
    append_program(merged, std::move(program));
}

static Program compile_frontend(const ProjectConfig& project, const BuildTarget& target) {
    Program program;
    std::unordered_set<std::string> loaded_modules;
    std::unordered_set<std::string> active_modules;
    const std::string entry_module_name = module_name_from_source_path(project, target.entry);
    loaded_modules.insert(entry_module_name);
    load_module_graph(project, target.entry, entry_module_name, program, loaded_modules, active_modules);
    SemanticAnalyzer sema;
    sema.analyze(program);
    return program;
}

static int command_check(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    compile_frontend(project, project.default_target);
    std::cout << "checked " << project.name << '\n';
    return 0;
}

static int command_build(const fs::path& project_dir, const std::optional<std::string>& target_name) {
    const ProjectConfig project = load_project(project_dir);
    const BuildTarget& target = resolve_target(project, target_name);
    Program program = compile_frontend(project, target);
    LLVMIRGenerator ir;
    const std::string llvm = ir.generate(program);

    fs::create_directories(target.output.parent_path());
    const std::string ll_path = target.output.string() + ".ll";
    const std::string obj_path = target.output.string() + ".obj";
    const std::string runtime_obj_path = target.output.string() + ".runtime.obj";
    const std::string exe_path = target.output.string() + ".exe";
    const fs::path runtime_source = fs::path(PINGGEN_SOURCE_ROOT) / "runtime" / "pinggen_runtime.cpp";

    {
        std::ofstream ll_output(ll_path);
        ll_output << llvm;
    }

    const std::string compile_command = "clang -c \"" + ll_path + "\" -o \"" + obj_path + "\"";
    if (std::system(compile_command.c_str()) != 0) {
        throw std::runtime_error("clang failed while compiling target '" + target.name + "'");
    }

    const std::string runtime_compile_command =
        "clang++ -c \"" + runtime_source.string() + "\" -o \"" + runtime_obj_path + "\"";
    if (std::system(runtime_compile_command.c_str()) != 0) {
        throw std::runtime_error("clang++ failed while compiling pinggen runtime");
    }

    const std::string link_command =
        "clang++ \"" + obj_path + "\" \"" + runtime_obj_path + "\" -o \"" + exe_path + "\"";
    if (std::system(link_command.c_str()) != 0) {
        throw std::runtime_error("clang failed while linking target '" + target.name + "'");
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
        fs::path path = fs::current_path();
        const fs::path executable_path = fs::absolute(fs::path(argv[0]));
        const std::string command_name = pinggen::command_name_from_path(executable_path);
        std::optional<std::string> target_name;
        std::optional<fs::path> bin_dir;
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
            if (target_name.has_value() || bin_dir.has_value()) {
                throw std::runtime_error("check does not support --target or --bin-dir");
            }
            return pinggen::command_check(path);
        }
        if (command == "build") {
            if (bin_dir.has_value()) {
                throw std::runtime_error("build does not support --bin-dir");
            }
            return pinggen::command_build(path, target_name);
        }
        if (command == "run") {
            if (bin_dir.has_value()) {
                throw std::runtime_error("run does not support --bin-dir");
            }
            return pinggen::command_run(path, target_name);
        }
        if (command == "targets") {
            if (target_name.has_value() || bin_dir.has_value()) {
                throw std::runtime_error("targets does not support --target or --bin-dir");
            }
            return pinggen::command_targets(path);
        }
        if (command == "doctor") {
            if (target_name.has_value() || bin_dir.has_value()) {
                throw std::runtime_error("doctor does not support --target or --bin-dir");
            }
            return pinggen::command_doctor(path, executable_path, command_name);
        }
        if (command == "install") {
            if (target_name.has_value()) {
                throw std::runtime_error("install does not support --target");
            }
            return pinggen::command_install(executable_path, bin_dir, command_name);
        }
        if (command == "setup") {
            if (target_name.has_value()) {
                throw std::runtime_error("setup does not support --target");
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

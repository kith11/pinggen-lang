#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

static std::string read_file(const fs::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("failed to open " + path.string());
    }
    std::ostringstream out;
    out << input.rdbuf();
    return out.str();
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
            fail(import_decl.location, "circular module import involving '" + imported_name + "'");
        }
        if (loaded_modules.contains(imported_name)) {
            continue;
        }
        const fs::path module_path = project.root / "src" / (imported_name + ".pg");
        if (!fs::exists(module_path)) {
            fail(import_decl.location, "missing module '" + imported_name + "' at " + module_path.string());
        }
        loaded_modules.insert(imported_name);
        load_module_graph(project, module_path, imported_name, merged, loaded_modules, active_modules);
    }
    active_modules.erase(module_name);
    append_program(merged, std::move(program));
}

static Program compile_frontend(const ProjectConfig& project) {
    Program program;
    std::unordered_set<std::string> loaded_modules;
    std::unordered_set<std::string> active_modules;
    const std::string entry_module_name = project.entry.stem().string();
    loaded_modules.insert(entry_module_name);
    load_module_graph(project, project.entry, entry_module_name, program, loaded_modules, active_modules);
    SemanticAnalyzer sema;
    sema.analyze(program);
    return program;
}

static int command_check(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    compile_frontend(project);
    std::cout << "checked " << project.name << '\n';
    return 0;
}

static int command_build(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    Program program = compile_frontend(project);
    LLVMIRGenerator ir;
    const std::string llvm = ir.generate(program);

    fs::create_directories(project.output.parent_path());
    const std::string ll_path = project.output.string() + ".ll";
    const std::string obj_path = project.output.string() + ".obj";
    const std::string exe_path = project.output.string() + ".exe";

    {
        std::ofstream ll_output(ll_path);
        ll_output << llvm;
    }

    const std::string compile_command = "clang -c \"" + ll_path + "\" -o \"" + obj_path + "\"";
    if (std::system(compile_command.c_str()) != 0) {
        throw std::runtime_error("clang failed while compiling " + project.name);
    }

    const std::string link_command = "clang \"" + obj_path + "\" -o \"" + exe_path + "\"";
    if (std::system(link_command.c_str()) != 0) {
        throw std::runtime_error("clang failed while linking " + project.name);
    }

    std::cout << "built " << exe_path << '\n';
    return 0;
}

static int command_run(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    command_build(project_dir);
    const std::string command = "\"" + project.output.string() + ".exe\"";
    return std::system(command.c_str());
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
            std::cerr << "usage: pinggen <new|check|build|run> [path]\n";
            return 1;
        }

        const std::string command = argv[1];
        const fs::path path = argc >= 3 ? fs::path(argv[2]) : fs::current_path();

        if (command == "new") {
            return pinggen::command_new(path);
        }
        if (command == "check") {
            return pinggen::command_check(path);
        }
        if (command == "build") {
            return pinggen::command_build(path);
        }
        if (command == "run") {
            return pinggen::command_run(path);
        }

        std::cerr << "unknown command: " << command << '\n';
        return 1;
    } catch (const pinggen::CompileError& error) {
        std::cerr << "compile error: " << error.what() << '\n';
        return 1;
    } catch (const std::exception& error) {
        std::cerr << "error: " << error.what() << '\n';
        return 1;
    }
}

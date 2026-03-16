#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

static Program compile_frontend(const fs::path& entry_path) {
    Lexer lexer(read_file(entry_path));
    Parser parser(lexer.tokenize());
    Program program = parser.parse();
    SemanticAnalyzer sema;
    sema.analyze(program);
    return program;
}

static int command_check(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    compile_frontend(project.entry);
    std::cout << "checked " << project.name << '\n';
    return 0;
}

static int command_build(const fs::path& project_dir) {
    const ProjectConfig project = load_project(project_dir);
    Program program = compile_frontend(project.entry);
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

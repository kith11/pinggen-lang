#include "pinggen/project.hpp"

#include <filesystem>
#include <fstream>
#include <string>

#include "pinggen/diagnostics.hpp"

namespace pinggen {

namespace {

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

}  // namespace

ProjectConfig load_project(const std::filesystem::path& project_dir) {
    const auto config_path = project_dir / "pinggen.toml";
    std::ifstream input(config_path);
    if (!input) {
        fail({1, 1}, "missing pinggen.toml at " + config_path.string());
    }

    ProjectConfig config;
    config.root = std::filesystem::absolute(project_dir);
    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        line = trim(line);
        if (line.empty() || line.front() == '#') {
            continue;
        }
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = trim(line.substr(0, eq));
        const std::string value = unquote(trim(line.substr(eq + 1)));
        if (section == "package" && key == "name") {
            config.name = value;
        } else if (section == "build" && key == "entry") {
            config.entry = config.root / value;
        } else if (section == "build" && key == "output") {
            config.output = config.root / value;
        }
    }

    if (config.name.empty()) {
        fail({1, 1}, "pinggen.toml is missing [package].name");
    }
    if (config.entry.empty()) {
        fail({1, 1}, "pinggen.toml is missing [build].entry");
    }
    if (config.output.empty()) {
        config.output = config.root / "build" / config.name;
    }
    return config;
}

void create_project(const std::filesystem::path& target_dir, const std::string& name) {
    std::filesystem::create_directories(target_dir / "src");
    std::filesystem::create_directories(target_dir / "build");

    std::ofstream config(target_dir / "pinggen.toml");
    config << "[package]\n";
    config << "name = \"" << name << "\"\n";
    config << "version = \"0.1.0\"\n\n";
    config << "[build]\n";
    config << "entry = \"src/main.pg\"\n";
    config << "output = \"build/" << name << "\"\n";

    std::ofstream message_file(target_dir / "message.txt");
    message_file << "hello from file\n";

    std::ofstream main_source(target_dir / "src" / "main.pg");
    main_source << "import std::{ io, fs }\n";
    main_source << "import model;\n";
    main_source << "import logic;\n\n";
    main_source << "func main() {\n";
    main_source << "    let job = Job { result: finish(0), label: \"pinggen\" };\n";
    main_source << "    let results: [Result; 3] = [Result::Pending, Result::Err(\"bad\"), Result::Ok(9)];\n";
    main_source << "    match job.result {\n";
    main_source << "        Result::Ok(code) => {\n";
    main_source << "            io::println(job.label);\n";
    main_source << "            io::println(code);\n";
    main_source << "        }\n";
    main_source << "        Result::Err(message) => {\n";
    main_source << "            io::println(message);\n";
    main_source << "        }\n";
    main_source << "        Result::Pending => {\n";
    main_source << "            io::println(\"pending\");\n";
    main_source << "        }\n";
    main_source << "    }\n";
    main_source << "    io::println(job.label_len());\n";
    main_source << "    match fs::read_to_string(\"message.txt\") {\n";
    main_source << "        FsResult::Ok(text) => {\n";
    main_source << "            io::println(text);\n";
    main_source << "        }\n";
    main_source << "        FsResult::Err(message) => {\n";
    main_source << "            io::println(message);\n";
    main_source << "        }\n";
    main_source << "    }\n";
    main_source << "    io::println(describe(results[2]));\n";
    main_source << "    io::println(describe(results[0]));\n";
    main_source << "}\n";

    std::ofstream model_source(target_dir / "src" / "model.pg");
    model_source << "import std::{ str }\n\n";
    model_source << "enum Result {\n";
    model_source << "    Ok(int),\n";
    model_source << "    Err(string),\n";
    model_source << "    Pending,\n";
    model_source << "}\n\n";
    model_source << "struct Job {\n";
    model_source << "    result: Result,\n";
    model_source << "    label: string,\n";
    model_source << "}\n\n";
    model_source << "impl Job {\n";
    model_source << "    func label_len(self) -> int {\n";
    model_source << "        return str::len(self.label);\n";
    model_source << "    }\n";
    model_source << "}\n";

    std::ofstream logic_source(target_dir / "src" / "logic.pg");
    logic_source << "import model;\n\n";
    logic_source << "func finish(code: int) -> Result {\n";
    logic_source << "    if code == 0 {\n";
    logic_source << "        return Result::Ok(7);\n";
    logic_source << "    }\n";
    logic_source << "    return Result::Err(\"bad\");\n";
    logic_source << "}\n\n";
    logic_source << "func describe(result: Result) -> string {\n";
    logic_source << "    match result {\n";
    logic_source << "        Result::Ok(value) => {\n";
    logic_source << "            if value > 5 {\n";
    logic_source << "                return \"ok\";\n";
    logic_source << "            }\n";
    logic_source << "            return \"small\";\n";
    logic_source << "        }\n";
    logic_source << "        Result::Err(message) => {\n";
    logic_source << "            return message;\n";
    logic_source << "        }\n";
    logic_source << "        Result::Pending => {\n";
    logic_source << "            return \"pending\";\n";
    logic_source << "        }\n";
    logic_source << "    }\n";
    logic_source << "}\n";
}

}  // namespace pinggen

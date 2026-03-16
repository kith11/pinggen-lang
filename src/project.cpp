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

    std::ofstream source(target_dir / "src" / "main.pg");
    source << "import std::{ io }\n\n";
    source << "struct Counter {\n";
    source << "    value: int,\n";
    source << "}\n\n";
    source << "impl Counter {\n";
    source << "    func bumped_copy(self, amount: int) -> Counter {\n";
    source << "        let mut copy = self;\n";
    source << "        copy.value = copy.value + amount;\n";
    source << "        return copy;\n";
    source << "    }\n";
    source << "    func bump(mut self, amount: int) -> int {\n";
    source << "        self.value = self.value + amount;\n";
    source << "        return self.value;\n";
    source << "    }\n";
    source << "    func total(self) -> int {\n";
    source << "        return self.value;\n";
    source << "    }\n";
    source << "}\n\n";
    source << "func main() {\n";
    source << "    let mut counter = Counter { value: 10 };\n";
    source << "    let preview = counter.bumped_copy(2);\n";
    source << "    io::println(preview.total());\n";
    source << "    io::println(counter.total());\n";
    source << "    io::println(counter.bump(5));\n";
    source << "    io::println(counter.total());\n";
    source << "}\n";
}

}  // namespace pinggen

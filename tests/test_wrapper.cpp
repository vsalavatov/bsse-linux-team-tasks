#include "test_wrapper.h"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <string>
#include <array>
#include <iostream>
#include <vector>
#include <sstream>

std::string execAndGetOutput(const char* cmd) {
    std::array<char, 512> buffer{};
    std::string result;
    std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        throw std::runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    return result;
}

static std::string stripPath(const std::string& path) {
    size_t pos;
    if ((pos = path.rfind('/')) != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

void runTest(int argc, char** argv,
             const std::set<std::pair<std::string, std::string>> &expected_dependency_graph,
             const std::set<std::pair<std::string, std::string>> &expected_symbol_import_scheme) {
    if (argc != 3) {
        std::cout << "Bad arguments.\n"
                     "Usage: " << argv[0] << " <path-to-my_ldd> <path-to-elf-binary>" << std::endl;
        std::cout << "TestFailed" << std::endl;
        return;
    }

    std::set<std::pair<std::string, std::string>> actual_dependency_graph;
    std::set<std::pair<std::string, std::string>> actual_symbol_import_scheme;

    const std::string dep_sym{" => "};
    const std::string import_sym{" <- "};

    try {
        std::string elfPath(argv[2]);
        std::string mylddPath(argv[1]);

        auto output = execAndGetOutput((mylddPath + " " + elfPath).c_str());
        std::vector<std::string> lines;

        {
            auto outputSS = std::stringstream{output};
            for (std::string line; std::getline(outputSS, line, '\n');) {
                lines.push_back(line);
            }
        }

        for (const std::string& line: lines) {
            size_t pos;
            if ((pos = line.find(dep_sym)) != std::string::npos) {
                std::string subject = stripPath(line.substr(0, pos));
                std::string dependency = stripPath(line.substr(pos + dep_sym.length()));
                if (dependency != "statically linked")
                    actual_dependency_graph.insert({ subject, dependency });
            } else if ((pos = line.find(" <- ")) != std::string::npos) {
                actual_symbol_import_scheme.insert({
                    line.substr(0, pos),
                    line.substr(pos + dep_sym.length())
                });
            }
        }
    } catch (std::exception &e) {
        std::cout << "TestFailed" << std::endl;
        std::cout << "Failed with exception: " << e.what() << std::endl;
        return;
    }

    if (actual_symbol_import_scheme == expected_symbol_import_scheme &&
        actual_dependency_graph == expected_dependency_graph) {
        std::cout << "TestPassed" << std::endl;
    } else {
        std::cout << "TestFailed" << std::endl;
        std::cout << "Diff:" << std::endl;
        for (const auto& [k, v] : expected_dependency_graph) {
            if (actual_dependency_graph.find({k, v}) == actual_dependency_graph.end()) {
                std::cout << "Expected dependency: " << k << dep_sym << v << std::endl;
                std::cout << "But it wasn't detected." << std::endl;
            }
        }
        for (const auto& [k, v] : actual_dependency_graph) {
            if (expected_dependency_graph.find({k, v}) == expected_dependency_graph.end()) {
                std::cout << "Detected unexpected dependency: " << k << dep_sym << v << std::endl;
            }
        }

        for (const auto& [k, v] : expected_symbol_import_scheme) {
            if (actual_symbol_import_scheme.find({k, v}) == actual_symbol_import_scheme.end()) {
                std::cout << "Expected import: " << k << import_sym << v << std::endl;
                std::cout << "But it wasn't detected." << std::endl;
            }
        }
        for (const auto& [k, v] : actual_symbol_import_scheme) {
            if (expected_symbol_import_scheme.find({k, v}) == expected_symbol_import_scheme.end()) {
                std::cout << "Detected unexpected import: " << k << import_sym << v << std::endl;
            }
        }
    }
}
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <cstdio>

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <dirent.h>
#include <sys/stat.h>

#include "elf_parser.h"


std::string find_lib_path(const std::string &libname, const std::unordered_set<std::string> &lib_search_paths) {
    for (const auto &path: lib_search_paths) {
        const std::string path_to_lib = path + "/" + libname;
        std::string err_message;
        if (is_valid_elf(path_to_lib.c_str(), err_message)) {
            return path_to_lib;
        }
    }
    return "";
}

void find_needed_libs(const char *name, const char *path, std::unordered_set<std::string> &all_libnames,
                      std::vector<Elf_Dependencies> &dependencies, const std::unordered_set<std::string> &lib_search_paths,
                      std::unordered_map<std::string, Elf>& found_elfs) {
    Elf elf = read_elf(path);

    dependencies.emplace_back(Elf_Dependencies{name, path, elf.needed_libnames});
    found_elfs.insert({name, elf});

    for (auto const &libname: elf.needed_libnames) {
        if (all_libnames.find(libname) == all_libnames.end()) {
            all_libnames.insert(libname);
            std::string libpath = find_lib_path(libname, lib_search_paths);
            if (libpath.empty()) {
                dependencies.emplace_back(Elf_Dependencies{libname, libpath, {}});
                continue;
            }
            find_needed_libs(libname.c_str(), libpath.c_str(), all_libnames, dependencies, lib_search_paths, found_elfs);
        }
    }
}

void collect_paths(std::unordered_set<std::string> &paths) {
    paths.insert({"/lib", "/usr/lib", "/usr/local/lib", "."});
    std::string dir = "/etc/ld.so.conf.d";

    DIR *dirp = opendir(dir.c_str());
    struct dirent *dp;
    struct stat filestat;
    std::string filepath;
    while ((dp = readdir(dirp)) != NULL) {
        filepath = dir + "/" + dp->d_name;
        if (stat(filepath.c_str(), &filestat) || S_ISDIR(filestat.st_mode))
            continue;
        std::ifstream fin(filepath);
        std::string line;
        while (std::getline(fin, line)) {
            if (!line.empty() && line[0] != '#')
                paths.insert(line);
        }
    }
    (void) closedir(dirp);
}

void get_import_schema(const char* name, std::unordered_map<std::string, Elf> &found_elfs,
                       std::unordered_map<std::string, std::string> &import_schema) {
    Elf elf = found_elfs[name];
    for (const auto& sym: elf.undef_symnames) {
        for (const auto& lib: elf.needed_libnames) {
            if (found_elfs.find(lib) != found_elfs.end()) {
                const auto &global_names = found_elfs[lib].global_symnames;
                if (global_names.find(sym) != global_names.end()) {
                    import_schema[sym] = lib;
                    break;
                }
            }
        }
        if (import_schema.find(sym) == import_schema.end()) {
            import_schema[sym] = "not found";
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        printf("usage: %s <elf-binary>\n", argv[0]);
        return 1;
    }
    
    const char *filename = argv[1];
    std::string err_message;
    if (!is_valid_elf(filename, err_message)) {
        std::cerr << err_message << std::endl;
        exit(1);
    }

    std::unordered_set<std::string> lib_search_paths;
    collect_paths(lib_search_paths);

    std::unordered_set<std::string> all_libnames;
    std::unordered_map<std::string, Elf> found_elfs;
    std::vector<Elf_Dependencies> dependencies;
    find_needed_libs(filename, filename, all_libnames, dependencies, lib_search_paths, found_elfs);

    for (const auto &elf: dependencies) {
        if (elf.path.empty()) {
            std::cout << elf.name << " => not found\n";
        } else if (elf.dependencies.empty()) {
            std::cout << elf.name << " => statically linked\n";
        } else {
            for (const auto &lib: elf.dependencies) {
                std::cout << elf.name << " => " << lib << "\n";
            }
        }
    }

    std::cout << "\n";

    std::unordered_map<std::string, std::string> import_schema;
    get_import_schema(filename, found_elfs, import_schema);
    for (auto [sym, lib]: import_schema) {
        std::cout << sym << " <- " << lib << "\n";
    }
    return 0;
}
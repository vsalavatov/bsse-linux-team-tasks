#ifndef ELF_PARSER_H
#define ELF_PARSER_H

#include <elf.h>

struct Elf {
    size_t sht_str_off;
    size_t dt_str_off;

    size_t dynsym_off;
    size_t dynsym_sz;
    size_t dynsect_off;
    size_t dynsect_sz;

    std::vector<std::string> needed_libnames;
    std::vector<std::string> undef_symnames;
    std::unordered_set<std::string> global_symnames;

};

struct Elf64: public Elf {
    Elf64_Ehdr hdr;
    std::vector<Elf64_Dyn> needed_libs;
};

struct Elf32: public Elf {
    Elf32_Ehdr hdr;
    std::vector<Elf32_Dyn> needed_libs;
};

struct Elf_Dependencies {
    std::string name;
    std::string path;
    std::vector<std::string> dependencies;
};

bool is_valid_elf(const char *filename, std::string &err_message);
Elf read_elf(const char *filename);

Elf64 read_elf64(std::vector<char> &elf_data);
Elf32 read_elf32(std::vector<char> &elf_data);
#endif

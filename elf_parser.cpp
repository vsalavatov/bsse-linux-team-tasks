#include <elf.h>

#include <cstring>

#include <iostream>
#include <fstream>
#include <vector>
#include <unordered_set>
#include <dirent.h>

#include "elf_parser.h"

bool is_valid_elf(const char *filename, std::string &err_message) {
    std::ifstream file(filename, std::ios::binary | std::ios::in);
    if (file.fail()) {
        err_message = "Failed to open file.";
        return false;
    }
    Elf64_Ehdr hdr;
    file.read((char *) &hdr, sizeof(hdr));

    const unsigned char expected_magic[] = {ELFMAG0, ELFMAG1, ELFMAG2, ELFMAG3};

    if (memcmp(hdr.e_ident, expected_magic, sizeof(expected_magic)) != 0) {
        err_message = "Target is not an ELF executable.";
        return false;
    }
    if (hdr.e_machine != EM_X86_64) {
        err_message = "Only x86-64 is supported.";
        return false;
    }
    if (hdr.e_ident[EI_CLASS] != ELFCLASS64 && hdr.e_ident[EI_CLASS] != ELFCLASS32) {
        err_message = "Invalid ELF class.";
        return false;
    }
    return true;
}

Elf read_elf(const char *filename) {
    std::vector<char> elf_data = [filename] {
        std::ifstream file(filename, std::ios::binary | std::ios::in);
        return std::vector<char>(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>{});
    }();

    Elf64 elf64 {};
    memmove(&elf64.hdr, elf_data.data(), sizeof(elf64.hdr));

    if (elf64.hdr.e_ident[EI_CLASS] == ELFCLASS64) {
        return read_elf64(elf_data);
    } else {
        return read_elf32(elf_data);
    }
}

Elf64 read_elf64(std::vector<char> &elf_data) {
    Elf64 elf64 {};
    const char *elf_ptr = elf_data.data();
    memmove(&elf64.hdr, elf_data.data(), sizeof(elf64.hdr));

    for (uint16_t i = 0; i < elf64.hdr.e_shnum; i++) {
        size_t offset = elf64.hdr.e_shoff + i * elf64.hdr.e_shentsize;
        Elf64_Shdr shdr;
        memmove(&shdr, elf_ptr + offset, sizeof(shdr));
        switch (shdr.sh_type) {
            case SHT_SYMTAB:
            case SHT_STRTAB:
                if (!elf64.sht_str_off)
                    elf64.sht_str_off = shdr.sh_offset;
                break;
            case SHT_DYNSYM:
                elf64.dynsym_off = shdr.sh_offset;
                elf64.dynsym_sz = shdr.sh_size;
                break;
            case SHT_DYNAMIC:
                elf64.dynsect_off = shdr.sh_offset;
                elf64.dynsect_sz = shdr.sh_size;
                break;
            default:
                break;
        }
    }

    for (size_t j = 0; j * sizeof(Elf64_Sym) < elf64.dynsym_sz; j++) {
        if (j == 0) continue; //  The .dynsym table begins with the standard NULL symbol
        Elf64_Sym sym;
        size_t offset = elf64.dynsym_off + j * sizeof(Elf64_Sym);
        memmove(&sym, elf_ptr + offset, sizeof(sym));
        std::string symname = elf_ptr + elf64.sht_str_off + sym.st_name;
        if (sym.st_shndx == STN_UNDEF && ELF64_ST_BIND(sym.st_info) != STB_WEAK) {
            elf64.undef_symnames.emplace_back(elf_ptr + elf64.sht_str_off + sym.st_name);
        } else if (ELF64_ST_BIND(sym.st_info) == STB_GLOBAL) {
            elf64.global_symnames.insert(symname);
        }
    }

    for (size_t j = 0; j * sizeof(Elf64_Sym) < elf64.dynsect_sz; j++) {
        Elf64_Dyn dyn;
        size_t offset = elf64.dynsect_off + j * sizeof(Elf64_Dyn);
        memmove(&dyn, elf_ptr + offset, sizeof(dyn));
        if (dyn.d_tag == DT_STRTAB) {
            elf64.dt_str_off = dyn.d_un.d_val;
        }
        if (dyn.d_tag == DT_NEEDED) {
            elf64.needed_libs.push_back(dyn);
        }
    }

    for (auto dyn: elf64.needed_libs) {
        elf64.needed_libnames.emplace_back(elf_ptr + elf64.dt_str_off + dyn.d_un.d_val);
    }

    return elf64;
}

Elf32 read_elf32(std::vector<char> &elf_data) {
    Elf32 elf32 {};
    const char *elf_ptr = elf_data.data();
    memmove(&elf32.hdr, elf_data.data(), sizeof(elf32.hdr));

    for (uint16_t i = 0; i < elf32.hdr.e_shnum; i++) {
        size_t offset = elf32.hdr.e_shoff + i * elf32.hdr.e_shentsize;
        Elf32_Shdr shdr;
        memmove(&shdr, elf_ptr + offset, sizeof(shdr));
        switch (shdr.sh_type) {
            case SHT_SYMTAB:
            case SHT_STRTAB:
                if (!elf32.sht_str_off)
                    elf32.sht_str_off = shdr.sh_offset;
                break;
            case SHT_DYNSYM:
                elf32.dynsym_off = shdr.sh_offset;
                elf32.dynsym_sz = shdr.sh_size;
                break;
            case SHT_DYNAMIC:
                elf32.dynsect_off = shdr.sh_offset;
                elf32.dynsect_sz = shdr.sh_size;
                break;
            default:
                break;
        }
    }

    for (size_t j = 0; j * sizeof(Elf32_Sym) < elf32.dynsym_sz; j++) {
        if (j == 0) continue; //  The .dynsym table begins with the standard NULL symbol
        Elf32_Sym sym;
        size_t offset = elf32.dynsym_off + j * sizeof(Elf32_Sym);
        memmove(&sym, elf_ptr + offset, sizeof(sym));
        std::string symname = elf_ptr + elf32.sht_str_off + sym.st_name;
        if (sym.st_shndx == STN_UNDEF && ELF32_ST_BIND(sym.st_info) != STB_WEAK) {
            elf32.undef_symnames.emplace_back(elf_ptr + elf32.sht_str_off + sym.st_name);
        } else if (ELF32_ST_BIND(sym.st_info) == STB_GLOBAL) {
            elf32.global_symnames.insert(symname);
        }
    }

    for (size_t j = 0; j * sizeof(Elf32_Sym) < elf32.dynsect_sz; j++) {
        Elf32_Dyn dyn;
        size_t offset = elf32.dynsect_off + j * sizeof(Elf32_Dyn);
        memmove(&dyn, elf_ptr + offset, sizeof(dyn));
        if (dyn.d_tag == DT_STRTAB) {
            elf32.dt_str_off = dyn.d_un.d_val;
        }
        if (dyn.d_tag == DT_NEEDED) {
            elf32.needed_libs.push_back(dyn);
        }
    }

    for (auto dyn: elf32.needed_libs) {
        elf32.needed_libnames.emplace_back(elf_ptr + elf32.dt_str_off + dyn.d_un.d_val);
    }

    return elf32;
}
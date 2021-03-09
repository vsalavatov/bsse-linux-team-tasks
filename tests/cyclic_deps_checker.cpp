#include "test_wrapper.h"

int main(int argc, char** argv) {
    std::set<std::pair<std::string, std::string>> expected_deps = {
            { "cyclic_deps_elf", "libeven.so" },
            { "cyclic_deps_elf", "libc.so.6" },
            { "libc.so.6", "ld-linux-x86-64.so.2" },
            { "libodd.so", "libeven.so" },
            { "libeven.so", "libodd.so" }
    };
    std::set<std::pair<std::string, std::string>> expected_import_scheme = {
            { "__libc_start_main", "libc.so.6" },
            { "is_even", "libeven.so" }
    };
    runTest(argc, argv, expected_deps, expected_import_scheme);
    return 0;
}

#include <iostream>
#include "test_wrapper.h"

int main(int argc, char** argv) {
    std::unordered_map<std::string, std::string> expected_deps = {
            { "no_deps_elf", "libc.so.6" },
            { "libc.so.6", "ld-linux-x86-64.so.2" },
    };
    std::unordered_map<std::string, std::string> expected_import_scheme = {
            { "__libc_start_main", "libc.so.6" }
    };
    runTest(argc, argv, expected_deps, expected_import_scheme);
    return 0;
}

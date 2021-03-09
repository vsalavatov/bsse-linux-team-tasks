#ifndef ELF_PARSER_TEST_WRAPPER_H
#define ELF_PARSER_TEST_WRAPPER_H

#include <string>
#include <set>

std::string execAndGetOutput(const char *cmd);
void runTest(int argc, char** argv,
             const std::set<std::pair<std::string, std::string>> &expected_dependency_graph,
             const std::set<std::pair<std::string, std::string>> &expected_symbol_import_scheme);

#endif //ELF_PARSER_TEST_WRAPPER_H

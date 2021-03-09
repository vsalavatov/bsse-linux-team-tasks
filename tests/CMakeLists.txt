SET(EXEC_WRAPPER test_wrapper.h test_wrapper.cpp)

add_subdirectory(no_deps)
add_executable(no_deps_checker no_deps_checker.cpp ${EXEC_WRAPPER})
add_test(NAME test_no_deps COMMAND $<TARGET_FILE:no_deps_checker> $<TARGET_FILE:my_ldd> $<TARGET_FILE:no_deps_elf>)

add_subdirectory(cyclic_deps)
add_executable(cyclic_deps_checker cyclic_deps_checker.cpp ${EXEC_WRAPPER})
add_test(NAME test_cyclic_deps COMMAND $<TARGET_FILE:cyclic_deps_checker> $<TARGET_FILE:my_ldd> $<TARGET_FILE:cyclic_deps_elf>)

set_tests_properties(test_no_deps test_cyclic_deps
        PROPERTIES
        PASS_REGULAR_EXPRESSION "TestPassed"
        FAIL_REGULAR_EXPRESSION "TestFailed")

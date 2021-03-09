add_library(even_tmp SHARED even.c)
set_target_properties(even_tmp PROPERTIES OUTPUT_NAME even)

add_library(odd SHARED odd.c)
target_link_libraries(odd PRIVATE even_tmp)

add_library(even SHARED even.c)
target_link_libraries(even PRIVATE odd)

add_executable(cyclic_deps_elf main_even.c)
target_link_libraries(cyclic_deps_elf PRIVATE even)

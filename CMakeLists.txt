set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED yes)
add_compile_options(-Wall -Wextra -pedantic)

add_executable(bablib src/bablib.cpp)

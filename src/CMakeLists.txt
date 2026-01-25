# Common compiler options
add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_23)
target_compile_options(compiler_flags INTERFACE -Wall -Wextra -Wpedantic -Wunused -Wconversion -Wsign-conversion -g)

# Core library for linking with all targets
add_library(lispy_core INTERFACE)
target_sources(lispy_core
    INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
)
target_link_libraries(lispy_core INTERFACE compiler_flags)

add_executable(lispy main.cpp)
target_link_libraries(lispy PRIVATE lispy_core)

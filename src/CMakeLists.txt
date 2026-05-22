# Common compiler options
add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_23)
target_compile_options(compiler_flags INTERFACE -Wall -Wextra -Wpedantic -Wunused -Wshadow -Wconversion -Wsign-conversion -g -O0 -fno-omit-frame-pointer)

# Core library for linking with all targets
add_library(lispy_core INTERFACE)
target_sources(lispy_core
    INTERFACE
    FILE_SET HEADERS
    BASE_DIRS include
)
target_link_libraries(lispy_core INTERFACE compiler_flags)

add_subdirectory(parser)
add_subdirectory(compiler)
add_subdirectory(assembler)
add_subdirectory(vm)

find_package(PkgConfig REQUIRED)
pkg_check_modules(Readline REQUIRED IMPORTED_TARGET readline)

add_executable(lispy main.cpp)
target_link_libraries(lispy PRIVATE lispy_core PkgConfig::Readline parser compiler)

cmake_minimum_required(VERSION 4.3.4)
project(lispy VERSION 0.0.1 LANGUAGES CXX)

# disable standard decaying and disable extensions
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# enable sanitizers for all targets
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_compile_options(-fsanitize=address,undefined -O0)
else()
  add_compile_options(-fsanitize=address,undefined)
endif()
add_link_options(-fsanitize=address,undefined)

# Common compiler options
add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_23)
target_compile_options(compiler_flags INTERFACE -Wall -Wextra -Wpedantic -Wunused -Wshadow -Wconversion -Wsign-conversion)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(Readline REQUIRED IMPORTED_TARGET readline)

add_executable(lispy)
target_sources(lispy
  PRIVATE
    src/main.cpp
  PRIVATE
    FILE_SET CXX_MODULES
    FILES
      src/analyser/constraint.cppm
      src/analyser/typechecker.cppm
      src/analyser/typed_expr.cppm
      src/common/entity.cppm
      src/common/expr.cppm
      src/common/formatter.cppm
      src/common/id.cppm
      src/common/resolved.cppm
      src/common/tag.cppm
      src/common/todo.cppm
      src/common/type.cppm
      src/common/type_storage.cppm
      src/parser/context.cppm
      src/parser/entity_storage.cppm
      src/parser/lowerer.cppm
      src/parser/parser.cppm
      src/parser/parsy.cppm
      src/parser/raw_ast.cppm
      src/parser/scope.cppm
      src/parser/shallow_ast.cppm
      src/parser/tag_storage.cppm
)
target_link_libraries(lispy PRIVATE
  compiler_flags
#   PkgConfig::Readline
)

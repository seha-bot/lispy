# Common compiler options
add_library(compiler_flags INTERFACE)
target_compile_features(compiler_flags INTERFACE cxx_std_23)
target_compile_options(compiler_flags INTERFACE -Wall -Wextra -Wpedantic -Wunused -Wshadow -Wconversion -Wsign-conversion)

# find_package(PkgConfig REQUIRED)
# pkg_check_modules(Readline REQUIRED IMPORTED_TARGET readline)

add_executable(lispy)
target_sources(lispy
  PRIVATE
    main.cpp
  PRIVATE
    FILE_SET CXX_MODULES
    FILES
      analyser/typechecker.cppm
      common/ast.cppm
      common/resolved.cppm
      common/todo.cppm
      common/type_storage.cppm
      parser/context.cppm
      parser/entity_storage.cppm
      parser/lowerer.cppm
      parser/parser.cppm
      parser/parsy.cppm
      parser/raw_ast.cppm
      parser/scope.cppm
      parser/shallow_ast.cppm
      parser/tag_storage.cppm
)
target_link_libraries(lispy PRIVATE
  compiler_flags
#   PkgConfig::Readline
)

add_library(lispy_lib)
target_sources(lispy_lib
  PUBLIC
    FILE_SET CXX_MODULES
    FILES
      analyser/constraint.cppm
      analyser/typechecker.cppm
      analyser/typed_expr.cppm
      common/entity.cppm
      common/expr.cppm
      common/formatter.cppm
      common/id.cppm
      common/resolved.cppm
      common/tag.cppm
      common/todo.cppm
      common/type.cppm
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
target_link_libraries(lispy_lib PUBLIC compiler_flags)

add_executable(lispy main.cpp)
target_link_libraries(lispy PRIVATE
  lispy_lib
)

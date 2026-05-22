add_library(compiler lowerer.cpp typechecker.cpp)
target_include_directories(compiler PRIVATE include)
target_link_libraries(compiler PRIVATE lispy_core)

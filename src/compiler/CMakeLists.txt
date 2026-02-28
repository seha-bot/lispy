add_library(compiler explorerv2.cpp name_resolver.cpp)
target_include_directories(compiler PRIVATE include)
target_link_libraries(compiler PRIVATE lispy_core)

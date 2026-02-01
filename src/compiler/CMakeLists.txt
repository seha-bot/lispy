add_library(compiler compiler.cpp)
target_include_directories(compiler PRIVATE include)
target_link_libraries(compiler PRIVATE lispy_core)

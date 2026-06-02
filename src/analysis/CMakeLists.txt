add_library(analysis typechecker.cpp)
target_include_directories(analysis PRIVATE include)
target_link_libraries(analysis PRIVATE lispy_core)

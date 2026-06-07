add_library(analyser typechecker.cpp)
target_include_directories(analyser PRIVATE include)
target_link_libraries(analyser PRIVATE lispy_core)

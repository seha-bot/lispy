add_library(parsing lowerer.cpp parser.cpp)
target_include_directories(parsing PRIVATE include)
target_link_libraries(parsing PRIVATE lispy_core)

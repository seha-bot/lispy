add_library(parser parser.cpp)
target_include_directories(parser PRIVATE include)
target_link_libraries(parser PRIVATE lispy_core)

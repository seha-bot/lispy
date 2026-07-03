add_library(emitter emitter.cpp)
target_include_directories(emitter PRIVATE include)
target_link_libraries(emitter PRIVATE lispy_core)

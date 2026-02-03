add_executable(assembler assembler.cpp)
target_include_directories(assembler PRIVATE include)
target_link_libraries(assembler PRIVATE lispy_core)

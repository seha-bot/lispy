add_executable(vm vm.cpp)
target_include_directories(vm PRIVATE include)
target_link_libraries(vm PRIVATE lispy_core)

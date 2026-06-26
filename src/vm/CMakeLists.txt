add_executable(vm vm.cpp)
# TODO: perhaps if you made this PUBLIC, clang-tidy would check the header also?
# Idk, check this https://iifx.dev/en/articles/457527172/effective-clang-tidy-setup-in-cmake-skipping-external-header-analysis
target_include_directories(vm PRIVATE include)
target_link_libraries(vm PRIVATE lispy_core)

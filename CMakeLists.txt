cmake_minimum_required(VERSION 3.23)
project(lispy VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_BUILD_TYPE Debug)

# disable standard decaying and disable extensions
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# enable sanitizers for all targets
add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g -O0)
add_link_options(-fsanitize=address,undefined)

add_subdirectory(src)

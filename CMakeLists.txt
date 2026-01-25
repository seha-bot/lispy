cmake_minimum_required(VERSION 3.23)
project(lispy VERSION 0.0.1 LANGUAGES CXX)

# disable standard decaying and disable extensions
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# enable sanitizers for all targets
add_compile_options(-fsanitize=address,undefined)
add_link_options(-fsanitize=address,undefined)

add_subdirectory(src)

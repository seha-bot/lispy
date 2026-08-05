find_package(Catch2 3 REQUIRED)

add_executable(typechecker_test analyser/typechecker_test.cpp)
target_link_libraries(typechecker_test PRIVATE
  lispy_lib
  Catch2::Catch2WithMain
)
add_test(NAME typechecker_test COMMAND typechecker_test)

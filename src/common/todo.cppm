module;

#include <cstdlib>
#include <iostream>
#include <source_location>

export module todo;

export [[noreturn]] inline void
todo(std::source_location location = std::source_location::current()) {
  std::cerr << "Todo called in " << location.file_name() << ':' << location.line() << ':'
            << location.column() << '\n'
            << location.function_name() << '\n';
  std::abort();
}

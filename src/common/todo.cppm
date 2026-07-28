module;

#include <exception>
#include <iostream>
#include <source_location>

export module todo;

export [[noreturn]] inline void
todo(std::source_location location = std::source_location::current()) {
  std::cerr << location.file_name() << ":" << location.line() << ":" << location.column()
            << ": unimplemented\n";
  std::terminate();
}

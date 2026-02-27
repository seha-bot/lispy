#ifndef TODO_HPP
#define TODO_HPP

#include <exception>
#include <iostream>
#include <source_location>

[[noreturn, deprecated]] inline void todo(const std::source_location location = std::source_location::current()) {
    std::cerr << location.file_name() << ":" << location.line() << ":" << location.column() << ": unimplemented\n";
    std::terminate();
}

#endif

#ifndef TYPECHECKER_HPP
#define TYPECHECKER_HPP

#include <expected>
#include <ostream>

#include "storage/resolved.hpp"

namespace analyser {

struct Error {
  friend std::ostream &operator<<(std::ostream &os, Error) { return os; }
};

std::expected<void, Error> typecheck(storage::ResolvedAST const &ast) noexcept;

} // namespace analyser

#endif

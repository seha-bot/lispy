#ifndef TYPECHECKER_HPP
#define TYPECHECKER_HPP

#include <expected>
#include <ostream>

#include "lowerer.hpp"

namespace typechecker {

struct Error {
    friend std::ostream& operator<<(std::ostream& os, Error) { return os; }
};

std::expected<void, Error> typecheck(compiler::TypeStorage& ts,
                                     compiler::ResolvedAST const& ast) noexcept;

}  // namespace typechecker

#endif

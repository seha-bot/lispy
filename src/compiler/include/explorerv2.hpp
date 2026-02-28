#ifndef EXPLORER_HPP
#define EXPLORER_HPP

#include <expected>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"

namespace definitions {

struct Error {
    friend std::ostream& operator<<(std::ostream& os, Error) { return os; }
};

std::expected<sast::Block, Error> discover(std::vector<rast::ExprPtr> ast);

}  // namespace definitions

#endif

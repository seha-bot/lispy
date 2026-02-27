#ifndef EXPLORER_HPP
#define EXPLORER_HPP

#include <expected>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"

namespace definitions {

struct Error {};

std::expected<std::vector<sast::Definition>, Error> discover(std::vector<rast::ExprPtr> ast);

}  // namespace definitions

#endif

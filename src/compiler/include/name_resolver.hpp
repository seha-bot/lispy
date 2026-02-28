#ifndef NAME_RESOLVER_HPP
#define NAME_RESOLVER_HPP

#include <expected>

#include "ast.hpp"

namespace names {

struct Error {};

std::expected<void, Error> resolve(sast::Block block);

}  // namespace names

#endif

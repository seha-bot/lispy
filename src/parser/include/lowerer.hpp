#ifndef LOWERER_HPP
#define LOWERER_HPP

#include "ast.hpp"
#include "storage/resolved.hpp"
#include <expected>
#include <ostream>
#include <string>
#include <vector>

namespace parser {

struct Error {
  friend std::ostream &operator<<(std::ostream &os, Error) { return os; }
};

std::expected<storage::ResolvedAST, Error> lower_ast(std::string filename,
                                                     std::vector<ast::RawExpr> ast) noexcept;

} // namespace parser

#endif

#ifndef LOWERER_HPP
#define LOWERER_HPP

#include "ast.hpp"
#include "storage/resolved.hpp"
#include "todo.hpp"
#include <expected>
#include <list>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

namespace compiler {

struct Error {
  friend std::ostream &operator<<(std::ostream &os, Error) { return os; }
};

std::expected<storage::ResolvedAST, Error> lower_ast(std::string filename,
                                                     std::vector<ast::RawExpr> ast) noexcept;

} // namespace compiler

#endif

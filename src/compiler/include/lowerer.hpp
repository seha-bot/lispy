#ifndef EXPLORER_HPP
#define EXPLORER_HPP

#include <expected>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"

namespace compiler {

struct Error {
    friend std::ostream& operator<<(std::ostream& os, Error) { return os; }
};

struct Result {
    ast::ModuleDefinition global_module;
    std::vector<ast::Entity> entities;
};

std::expected<Result, Error> lower_ast(std::string filename, std::vector<ast::Expr> ast) noexcept;

}  // namespace compiler

#endif

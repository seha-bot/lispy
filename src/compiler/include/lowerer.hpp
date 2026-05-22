#ifndef LOWERER_HPP
#define LOWERER_HPP

#include <expected>
#include <list>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "todo.hpp"
#include "type_storage.hpp"

namespace compiler {

struct Error {
    friend std::ostream& operator<<(std::ostream& os, Error) { return os; }
};

struct ResolvedAST {
    ast::ModuleDefinition global_module;
    std::unique_ptr<TypeStorage> ts;
    std::vector<ast::Entity> entities;
};

std::expected<ResolvedAST, Error> lower_ast(std::string filename,
                                            std::vector<ast::RawExpr> ast) noexcept;

}  // namespace compiler

#endif

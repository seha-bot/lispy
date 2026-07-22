#ifndef SHALLOW_AST_HPP
#define SHALLOW_AST_HPP

#include "raw_ast.hpp"

// Shallow entites are partially-compiled entities.
namespace parser::shallow_ast {

struct ShallowValueDefinition {
  std::string name;
  raw_ast::Expr raw_value;
};

struct ShallowMergedValueDefinition {
  std::string name;
  raw_ast::Expr raw_type_signature;
  raw_ast::Expr raw_value;
};

struct ShallowTypeFormDefinition {
  std::string name;
  raw_ast::Expr raw_type;
};

struct ShallowValueDeclaration {
  std::string name;
  raw_ast::Expr raw_type_signature;
};

struct ShallowModuleDefinition {
  std::string name;
  raw_ast::Expr raw_entities;
};

// TODO: This must be in the same order as EntityBase from ast.hpp.
// Automate this to remove human error.
using ShallowEntityBase =
    std::variant<ShallowValueDeclaration, ShallowValueDefinition, ShallowMergedValueDefinition,
                 ShallowTypeFormDefinition, ShallowModuleDefinition>;

// TODO: rename to entity
struct ShallowEntity : ShallowEntityBase {
  using ShallowEntityBase::variant;

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

} // namespace parser::shallow_ast

#endif

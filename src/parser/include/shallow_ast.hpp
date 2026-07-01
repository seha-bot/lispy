#ifndef SHALLOW_AST_HPP
#define SHALLOW_AST_HPP

#include "ast.hpp"

// Shallow entites are partially-compiled entities.
namespace parser::shallow_ast {

struct ShallowValueDefinition {
  std::string name;
  ast::RawExpr raw_value;
};

struct ShallowMergedValueDefinition {
  std::string name;
  ast::RawExpr raw_type_signature;
  ast::RawExpr raw_value;
};

struct ShallowTypeFormDefinition {
  std::string name;
  ast::RawExpr raw_type;
};

struct ShallowValueDeclaration {
  std::string name;
  ast::RawExpr raw_type_signature;
};

struct ShallowModuleDefinition {
  std::string name;
  ast::RawExpr raw_entities;
};

using ShallowEntityBase =
    std::variant<ShallowMergedValueDefinition, ShallowValueDefinition, ShallowTypeFormDefinition,
                 ShallowValueDeclaration, ShallowModuleDefinition>;

struct ShallowEntity : ShallowEntityBase {
  using ShallowEntityBase::variant;

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

} // namespace parser::shallow_ast

#endif

module;

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

export module ast;

import entity;
import tag;
import type;

export namespace ast {

namespace expr {
struct Expr;
}

namespace entity {

struct ValueDeclaration {
  std::string name;
  type::Id type_signature;
};

struct ValueDefinition {
  std::string name;
  std::unique_ptr<expr::Expr> value;
};

struct MergedValueDefinition {
  std::string name;
  type::Id type_signature;
  std::unique_ptr<expr::Expr> value;
};

struct TypeFormDefinition {
  std::string name;
  type::Id type;
};

struct Binding {
  std::string name;
  std::optional<type::Id> type;
};

struct TypeBinding {
  std::string name;
};

struct ModuleEntity;
struct ModuleDefinition {
  std::string name;
};

using ModuleEntityBase = std::variant<ValueDeclaration, ValueDefinition, MergedValueDefinition,
                                      TypeFormDefinition, ModuleDefinition>;
struct ModuleEntity : ModuleEntityBase {
  using ModuleEntityBase::variant;

  std::string &name() {
    return std::visit([](auto &e) -> std::string & { return e.name; }, *this);
  }

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

using TypedEntityBase =
    std::variant<ValueDeclaration, ValueDefinition, MergedValueDefinition, Binding>;
struct TypedEntity : TypedEntityBase {
  using TypedEntityBase::variant;
};

} // namespace entity

namespace expr {

struct Expr;

struct Application {
  std::unique_ptr<Expr> function;
  std::unique_ptr<Expr> argument;
};

struct Case {
  struct Pattern {
    tag::Id tag;
    std::vector<entity::Binding> bindings;
  };

  struct Choice;

  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

struct Variant {
  tag::Id tag_id;
  std::optional<std::unique_ptr<Expr>> value;
};

// This looks a lot like Variant above. Hmmm...
struct TaggedValue {
  tag::Id tag_id;
  std::unique_ptr<Expr> value;
};

struct Pack {
  std::vector<TaggedValue> tagged_values;
};

struct Lambda {
  std::vector<std::reference_wrapper<entity::Binding const>> captures;
  std::unique_ptr<entity::Binding> binding;
  std::unique_ptr<Expr> body;
};

struct TVLambda {
  std::unique_ptr<Expr> body;
};

struct ValueReference {
  // std::variant<entity::ValueDeclaration const *, entity::ValueDefinition const *,
  //              entity::MergedValueDefinition const *>
  //     entity;
  // TODO: This is guaranteed to be one from the comment above.
  ::entity::Id value_entity_id;
};

struct BindingReference {
  std::reference_wrapper<entity::Binding const> binding;
};

using ExprBase = std::variant<Application, Case, Variant, Pack, Lambda, TVLambda, ValueReference,
                              BindingReference>;
struct Expr : ExprBase {
  using ExprBase::variant;
};

struct Case::Choice {
  Pattern pattern;
  Expr arm;
};

} // namespace expr

} // namespace ast

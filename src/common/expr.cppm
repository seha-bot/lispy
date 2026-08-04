module;

#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

export module expr;

import entity;
import id;

export namespace expr {

struct Expr;

struct Application {
  std::unique_ptr<Expr> function;
  std::unique_ptr<Expr> argument;
};

struct Case {
  struct Pattern {
    id::TagId tag;
    std::vector<entity::Binding> bindings;
  };

  struct Choice;

  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

struct Variant {
  id::TagId tag_id;
  std::optional<std::unique_ptr<Expr>> value;
};

// This looks a lot like Variant above. Hmmm...
struct TaggedValue {
  id::TagId tag_id;
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
  id::EntityId value_entity_id;
};

struct BindingReference {
  std::reference_wrapper<entity::Binding const> binding;
};

using ExprBase = std::variant<Application, Case, Variant, Pack, Lambda, TVLambda, ValueReference,
                              BindingReference>;
struct Expr : ExprBase {
  using ExprBase::ExprBase;
};

struct Case::Choice {
  Pattern pattern;
  Expr arm;
};

} // namespace expr

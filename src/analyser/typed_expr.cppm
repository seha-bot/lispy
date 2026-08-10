module;

#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

export module typed_expr;

import entity;
import expr;
import id;

export namespace typed_expr {

struct Expr;

struct Typed {
  id::TypeId type_id;
};

struct Application : Typed {
  std::unique_ptr<Expr> function;
  std::unique_ptr<Expr> argument;
};

struct Choice;

struct Case : Typed {
  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

struct Variant : Typed {
  id::TagId tag_id;
  std::optional<std::unique_ptr<Expr>> value;
};

// This looks a lot like Variant above. Hmmm...
struct TaggedValue {
  id::TagId tag_id;
  std::unique_ptr<Expr> value;
};

struct Pack : Typed {
  std::vector<TaggedValue> tagged_values;
};

struct Lambda : Typed {
  std::vector<std::reference_wrapper<entity::Binding const>> captures;
  std::unique_ptr<entity::Binding> binding;
  std::unique_ptr<Expr> body;
};

struct TVLambda : Typed {
  std::unique_ptr<Expr> body;
};

struct ValueReference : Typed {
  // std::variant<entity::ValueDeclaration const *, entity::ValueDefinition const *,
  //              entity::MergedValueDefinition const *>
  //     entity;
  // TODO: This is guaranteed to be one from the comment above.
  id::EntityId value_entity_id;
};

struct BindingReference : Typed {
  std::reference_wrapper<entity::Binding const> binding;
};

struct Conversion : Typed {
  std::unique_ptr<Expr> expr;
};

using ExprBase = std::variant<Application, Case, Variant, Pack, Lambda, TVLambda, ValueReference,
                              BindingReference, Conversion>;
struct Expr : ExprBase {
  using ExprBase::ExprBase;

  id::TypeId type_id() const {
    return std::visit([](Typed const &t) { return t.type_id; }, *this);
  }
};

struct Choice {
  expr::Pattern pattern;
  Expr arm;
};

} // namespace typed_expr

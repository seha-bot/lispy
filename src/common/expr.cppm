module;

#include <functional>
#include <memory>
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

struct Pattern;

struct TagPattern {
  id::TagId tag_id;
};

struct TaggedValuePattern {
  id::TagId tag_id;
  std::unique_ptr<Pattern> rest;
};

struct PackPattern {
  std::vector<TaggedValuePattern> tagged_values;
};

struct BindingPattern {
  std::unique_ptr<entity::Binding> binding;
};

using PatternBase = std::variant<TagPattern, TaggedValuePattern, PackPattern, BindingPattern>;
struct Pattern : PatternBase {
  using PatternBase::PatternBase;
};

struct Choice;

struct Case {
  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

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

using ExprBase = std::variant<Application, Case, TaggedValue, Pack, Lambda, TVLambda,
                              ValueReference, BindingReference>;
struct Expr : ExprBase {
  using ExprBase::ExprBase;
};

struct Choice {
  Pattern pattern;
  Expr arm;
};

} // namespace expr

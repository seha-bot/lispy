#ifndef AST_HPP
#define AST_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ast {

struct TypeId {
  /// Special value which represents the unit type.
  static const TypeId unit_id;

  std::size_t value;
};

constexpr TypeId TypeId::unit_id = {.value = static_cast<std::size_t>(-1)};

struct Tag {
  bool operator==(Tag const &) const = default;
  std::string name;
};

struct TagId {
  auto operator<=>(TagId const &) const = default;
  std::size_t value;
};

struct EntityId {
  bool operator==(EntityId const &) const = default;
  std::size_t value;
};

namespace expr {
struct Expr;
}

namespace entity {

struct ValueDeclaration {
  std::string name;
  TypeId type_signature;
};

struct ValueDefinition {
  std::string name;
  std::unique_ptr<expr::Expr> value;
};

struct MergedValueDefinition {
  std::string name;
  TypeId type_signature;
  std::unique_ptr<expr::Expr> value;
};

struct TypeFormDefinition {
  std::string name;
  TypeId type;
};

struct Binding {
  std::string name;
  std::optional<TypeId> type;
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
    TagId tag;
    std::vector<entity::Binding> bindings;
  };

  struct Choice;

  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

struct Variant {
  TagId tag_id;
  std::optional<std::unique_ptr<Expr>> value;
};

// This looks a lot like Variant above. Hmmm...
struct TaggedValue {
  TagId tag_id;
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
  EntityId value_entity_id;
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

namespace type {

struct Arrow {
  TypeId from_id;
  TypeId to_id;
};

// This is indexed by De Bruijn indices to simplify merging.
struct ForAll {
  TypeId type_id;
};

struct DeBruijnIndex {
  std::size_t value;
};

struct Element {
  TagId tag_id;
  TypeId type_id;
};

struct Variant {
  // FIX: DO NOT RELY ON THIS PLEASE!
  // It is safe to assume that this is sorted by tag_id.
  std::vector<Element> elements;
};

struct Struct {
  // FIX: DO NOT RELY ON THIS PLEASE!
  // It is safe to assume that this is sorted by tag_id.
  std::vector<Element> elements;
};

struct Application {
  TypeId function_id;
  TypeId argument_id;
};

// Each object represents a unique variable.
struct Variable {};

struct NamedTypeReference {
  // TODO: Strongly type this so that it's guaranteed it represents a TypeFormDefinition.
  EntityId definition_id;
};

using UnnamedBase = std::variant<Arrow, ForAll, DeBruijnIndex, Variant, Struct, Application,
                                 Variable, NamedTypeReference>;
struct Unnamed : UnnamedBase {
  using UnnamedBase::variant;
};

// FIX: Dude... this is unused...
struct Named {
  Unnamed type;
  std::reference_wrapper<entity::TypeFormDefinition const> definition;
};

using TypeBase = std::variant<Unnamed, Named>;
struct Type : TypeBase {
  using TypeBase::variant;

  entity::TypeFormDefinition const *definition() const {
    struct Visitor {
      entity::TypeFormDefinition const *operator()(Unnamed const &) { return nullptr; }
      entity::TypeFormDefinition const *operator()(Named const &t) { return &t.definition.get(); }
    };
    return std::visit(Visitor{}, *this);
  }

  Unnamed const &unnamed_part() const {
    struct Visitor {
      Unnamed const &operator()(Unnamed const &t) { return t; }
      Unnamed const &operator()(Named const &t) { return t.type; }
    };
    return std::visit(Visitor{}, *this);
  }
};

} // namespace type

} // namespace ast

template <> struct std::hash<ast::EntityId> {
  static std::size_t operator()(ast::EntityId const &id) { return id.value; }
};

#endif

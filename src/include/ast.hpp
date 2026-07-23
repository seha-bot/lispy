#ifndef AST_HPP
#define AST_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ast {

// TODO: make a typed wrapper around this so, for example, lambda knows that
// the entity it holds is a Lambda::Parameter.
struct EntityId {
  bool operator==(EntityId const &) const = default;
  std::size_t value;
};

struct Tag {
  bool operator==(Tag const &) const = default;
  std::string name;
};

struct TagId {
  auto operator<=>(TagId const &) const = default;
  // TODO: Rename to value.
  std::size_t id;
};

struct TypeId {
  /// Special value which represents the unit type.
  static const TypeId unit_id;

  std::size_t value;
};

constexpr TypeId TypeId::unit_id = {.value = static_cast<std::size_t>(-1)};

struct EntityReference {
  EntityId id;
};

struct Expr;

struct Call {
  std::unique_ptr<Expr> callee;
  std::vector<Expr> arguments;
};

struct Case {
  struct Pattern {
    TagId tag;
    std::vector<EntityId> bindings;
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

struct Binding {
  std::string name;
  std::optional<TypeId> type;
};

struct TypeBinding {
  std::string name;
  // std::optional<Kind> kind;
};

struct Lambda {
  std::vector<EntityId> captures;
  // TODO: rename to binding_id.
  EntityId parameter;
  std::unique_ptr<Expr> body;
};

struct TVLambda {
  // This is a TypeBinding
  EntityId binding_id;
  std::unique_ptr<Expr> body;
};

using ExprBase = std::variant<Call, Case, Variant, Pack, Lambda, TVLambda, EntityReference>;
struct Expr : ExprBase {
  using ExprBase::variant;
};

struct Case::Choice {
  Pattern pattern;
  Expr arm;
};

struct ValueDefinition {
  std::string name;
  Expr value;
};

struct TypeFormDefinition {
  std::string name;
  TypeId type;
};

struct ValueDeclaration {
  std::string name;
  TypeId type_signature;
};

struct MergedValueDefinition {
  std::string name;
  TypeId type_signature;
  Expr value;
};

struct ModuleDefinition {
  std::string name;
  std::vector<EntityId> entities;
};

// Entities have names.
// FIX: Entities need a stricter definition.
// Look what your lies have allowed:
// (def val
//   (pack
//     (:a Bool)
//     (:b Bool)))
// TODO: Should bindings be entities? They don't behave like other entities because
// they are never shallow.
using EntityBase = std::variant<ValueDeclaration, ValueDefinition, MergedValueDefinition,
                                TypeFormDefinition, ModuleDefinition, Binding, TypeBinding>;
struct Entity : EntityBase {
  using EntityBase::variant;

  std::string &name() {
    return std::visit([](auto &e) -> std::string & { return e.name; }, *this);
  }

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

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

struct NamedReference {
  // This must always be a TypeFormDefinition.
  EntityId definition_id;
};

using UnnamedBase = std::variant<Arrow, ForAll, DeBruijnIndex, Variant, Struct, Application,
                                 Variable, NamedReference>;
struct Unnamed : UnnamedBase {
  using UnnamedBase::variant;
};

struct Named {
  Unnamed type;
  // TODO: maybe unneded.
  EntityId definition_id;
};

using TypeBase = std::variant<Unnamed, Named>;
struct Type : TypeBase {
  using TypeBase::variant;

  std::optional<EntityId> definition_id() const {
    struct Visitor {
      std::optional<EntityId> operator()(Unnamed const &) { return std::nullopt; }
      std::optional<EntityId> operator()(Named const &t) { return t.definition_id; }
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
  static std::size_t operator()(ast::EntityId const &eid) { return eid.value; }
};

#endif

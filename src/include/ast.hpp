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
  bool operator==(TagId const &) const = default;
  std::size_t id;
};

struct TypeId {
  /// Special value which represents the unit type.
  static const TypeId unit_id;

  std::size_t value;
};

constexpr TypeId TypeId::unit_id = {.value = static_cast<std::size_t>(-1)};

struct NamedType {
  EntityId definition;
};

struct TypeArrow {
  TypeId from;
  TypeId to;
};

struct TypeVariant {
  struct Element {
    TagId tag;
    TypeId type;
  };

  // This is supposed to be sorted by tag.
  std::vector<Element> elements;
};

struct TypeStruct {
  // TODO: This is the same as the one in TypeVariant.
  struct Element {
    TagId tag;
    TypeId type;
  };

  std::vector<Element> elements;
};

struct TypeApplication {
  TypeId function;
  std::vector<TypeId> arguments;
};

// Each object represents a unique variable.
struct TypeVariable {};

using TypeBase =
    std::variant<NamedType, TypeArrow, TypeVariant, TypeStruct, TypeApplication, TypeVariable>;
struct Type : TypeBase {
  using TypeBase::variant;
};

struct Call;
struct Case;
struct Constructor;
struct Lambda;

struct EntityReference {
  EntityId id;
};

using ExprBase = std::variant<Call, Case, Constructor, Lambda, EntityReference>;
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

struct Constructor {
  TagId tag;
  TypeId type;
  std::optional<std::unique_ptr<Expr>> argument;
};

struct Binding {
  std::string name;
  std::optional<TypeId> type;
};

struct Lambda {
  std::vector<EntityId> captures;
  EntityId parameter;
  std::unique_ptr<Expr> body;
};

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
using EntityBase = std::variant<ValueDefinition, TypeFormDefinition, ValueDeclaration,
                                MergedValueDefinition, ModuleDefinition, Binding>;
struct Entity : EntityBase {
  using EntityBase::variant;

  std::string &name() {
    return std::visit([](auto &e) -> std::string & { return e.name; }, *this);
  }

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

} // namespace ast

template <> struct std::hash<ast::EntityId> {
  static std::size_t operator()(ast::EntityId const &eid) { return eid.value; }
};

#endif

#ifndef AST_HPP
#define AST_HPP

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

// TODO: wtf?
namespace storage {
struct RepresentativeSets;
struct TypeStorage;
} // namespace storage

namespace ast {

// TODO: make a typed wrapper around this so, for example, lambda knows that
// the entity it holds is a Lambda::Parameter.
struct EntityId {
  bool operator==(EntityId const &) const = default;
  std::size_t value;
};

struct Label {
  bool operator==(Label const &) const = default;
  std::string name;
};

struct LabelId {
  bool operator==(LabelId const &) const = default;
  std::size_t id;
};

struct TypeId {
  explicit TypeId(std::size_t id, storage::RepresentativeSets &rep) : m_id(id), m_rep(&rep) {}

  bool operator==(TypeId const &that) const;
  std::string to_string() const;

private:
  friend storage::RepresentativeSets;
  friend storage::TypeStorage;
  std::size_t m_id;
  storage::RepresentativeSets *m_rep;
};

struct NamedType {
  bool operator==(NamedType const &) const = default;
  EntityId definition;
};

struct TypeArrow {
  bool operator==(TypeArrow const &) const = default;
  TypeId from;
  TypeId to;
};

struct TypeVariant {
  struct Element {
    bool operator==(Element const &) const = default;
    LabelId tag;
    // If type is std::nullopt, then this is treated as if it is a type which contains one value.
    std::optional<TypeId> type;
  };

  bool operator==(TypeVariant const &) const = default;
  std::vector<Element> elements;
};

struct TypeTuple {
  bool operator==(TypeTuple const &) const = default;
  std::vector<TypeId> elements;
};

struct TypeApplication {
  bool operator==(TypeApplication const &) const = default;
  TypeId function;
  std::vector<TypeId> arguments;
};

struct TypeVariable {
  bool operator==(TypeVariable const &) const = default;
  std::size_t id;
};

using TypeBase =
    std::variant<NamedType, TypeArrow, TypeVariant, TypeTuple, TypeApplication, TypeVariable>;
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
    LabelId tag;
    std::vector<EntityId> bindings;
  };

  struct Choice;

  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

struct Constructor {
  LabelId tag;
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

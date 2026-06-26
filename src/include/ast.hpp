#ifndef AST_HPP
#define AST_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// TODO: wtf?
namespace storage {
struct RepresentativeSets;
struct TypeStorage;
} // namespace storage

namespace ast {

struct Source {
  int line;
  int col;
};

struct Atom {
  Atom(std::string value, Source) : m_name(std::move(value)) {}

  std::string const &name() const { return m_name; }
  std::string &name() { return m_name; }

private:
  std::string m_name;
};

struct Number {
  Number(std::int64_t value, Source) : m_value(value) {}

  std::int64_t value() const { return m_value; }

private:
  std::int64_t m_value;
};

struct List;

using RawExprBase = std::variant<Atom, List, Number>;
struct RawExpr;

struct List {
  List();
  List(std::vector<RawExpr> list, Source);
  bool empty() const;
  std::size_t size() const;
  RawExpr &operator[](std::size_t i);
  std::vector<RawExpr> &elements();

private:
  std::vector<RawExpr> m_elements;
};

struct RawExpr : RawExprBase {
  using RawExprBase::variant;
};

inline List::List() = default;
inline List::List(std::vector<RawExpr> list, Source) : m_elements(std::move(list)) {}
inline bool List::empty() const { return m_elements.empty(); }
inline std::size_t List::size() const { return m_elements.size(); }
inline RawExpr &List::operator[](std::size_t i) { return m_elements[i]; }
inline std::vector<RawExpr> &List::elements() { return m_elements; }

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

struct Kind {
  bool operator==(Kind const &) const = default;
  std::optional<std::pair<std::shared_ptr<Kind>, std::shared_ptr<Kind>>> arrow;
};

// TODO: this is more of a "NamedType"
struct TypeReference {
  bool operator==(TypeReference const &) const = default;
  EntityId definition;
};

struct TTLambda {
  bool operator==(TTLambda const &) const = default;
  struct Parameter {
    std::string name;
  };

  EntityId parameter;
  std::optional<Kind> parameter_kind;
  TypeId body;
};

struct TypeArrow {
  bool operator==(TypeArrow const &) const = default;
  TypeId from;
  TypeId to;
};

struct TypeVariant {
  bool operator==(TypeVariant const &) const = default;
  std::vector<std::pair<LabelId, std::optional<TypeId>>> elements;
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

using TypeBase = std::variant<TypeReference, TTLambda, TypeArrow, TypeVariant, TypeTuple,
                              TypeApplication, TypeVariable>;
struct Type : TypeBase {
  using TypeBase::variant;
};

struct Call;
struct Case;
struct LabelCall;
struct Lambda;
struct TVLambda;

struct EntityReference {
  EntityId id;
};

using ExprBase = std::variant<Number, Call, Case, LabelCall, Lambda, TVLambda, EntityReference>;
struct Expr;

struct Call {
  std::unique_ptr<Expr> callee;
  std::vector<Expr> arguments;
};

struct Case {
  struct Pattern {
    LabelId name;
    std::optional<EntityId> variable;
  };

  struct Alternative;

  std::unique_ptr<Expr> scrutinee;
  std::vector<Alternative> alternatives;
};

struct LabelCall {
  // TODO: feel free to rename it now to label_id
  LabelId callee;
  TypeId type;
  std::optional<std::unique_ptr<Expr>> argument;
};

struct Lambda {
  // TODO: Perhaps this and the binding for a pattern can be the same type?
  struct Parameter {
    std::string name;
    std::optional<TypeId> type;
  };

  std::vector<EntityId> captures;
  EntityId parameter;
  std::unique_ptr<Expr> body;
};

struct TVLambda {
  struct Parameter {
    std::string name;
  };

  EntityId parameter;
  std::optional<Kind> parameter_kind;
  std::unique_ptr<Expr> body;
};

struct Expr : ExprBase {
  using ExprBase::variant;
};

struct Case::Alternative {
  Pattern pattern;
  Expr arm;
};

struct ShallowValueDefinition {
  std::string name;
  RawExpr raw_value;
};

struct ShallowMergedValueDefinition {
  std::string name;
  RawExpr raw_type_signature;
  RawExpr raw_value;
};

struct ShallowTypeFormDefinition {
  std::string name;
  RawExpr raw_type;
};

struct ShallowValueDeclaration {
  std::string name;
  RawExpr raw_type_signature;
};

struct ShallowModuleDefinition {
  std::string name;
  List raw_entities;
};

// Shallow entites are partially-compiled entities.
using ShallowEntityBase =
    std::variant<ShallowMergedValueDefinition, ShallowValueDefinition, ShallowTypeFormDefinition,
                 ShallowValueDeclaration, ShallowModuleDefinition>;
struct ShallowEntity : ShallowEntityBase {
  using ShallowEntityBase::variant;
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

// TODO: rename to PatternValueDefinition
struct PatternBinding {
  std::string name;
};

// Entities have names.
using EntityBase = std::variant<ValueDefinition, TypeFormDefinition, ValueDeclaration,
                                MergedValueDefinition, ModuleDefinition, Lambda::Parameter,
                                TVLambda::Parameter, TTLambda::Parameter, PatternBinding>;
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

module;

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <variant>

export module entity;

import id;

export namespace entity {

struct ValueDeclaration {
  std::string name;
  id::TypeId type_signature;
};

template <typename Expr> struct ValueDefinition {
  std::string name;
  std::unique_ptr<Expr> value;
};

template <typename Expr> struct MergedValueDefinition {
  std::string name;
  id::TypeId type_signature;
  std::unique_ptr<Expr> value;
};

struct TypeFormDefinition {
  std::string name;
  id::TypeId type;
};

struct Binding {
  std::string name;
  std::optional<id::TypeId> type;
};

struct TypeBinding {
  std::string name;
};

struct ModuleDefinition {
  std::string name;
};

template <typename Expr>
using ModuleEntityBase =
    std::variant<ValueDeclaration, ValueDefinition<Expr>, MergedValueDefinition<Expr>,
                 TypeFormDefinition, ModuleDefinition>;

template <typename Expr> struct ModuleEntity : ModuleEntityBase<Expr> {
  using ModuleEntityBase<Expr>::ModuleEntityBase;

  std::string &name() {
    return std::visit([](auto &e) -> std::string & { return e.name; }, *this);
  }

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

} // namespace entity

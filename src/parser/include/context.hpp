#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "ast.hpp"
#include "storage/entity_storage.hpp"
#include "storage/type_storage.hpp"
#include <optional>

namespace parser {

struct Scope {
  std::optional<ast::EntityId> lookup(std::string_view name) const {
    auto it = entities.find(std::string(name));
    if (it == entities.end()) {
      if (not parent) {
        return std::nullopt;
      }
      return parent->lookup(name);
    }
    return it->second;
  }

  std::unordered_map<std::string, ast::EntityId> entities;
  std::shared_ptr<Scope> parent;
};

struct Context {
  Context(storage::TypeStorage &ts_, storage::EntityStorage &es_, std::shared_ptr<Scope> scope)
      : ts(ts_), es(es_), m_scope(std::move(scope)) {}

  Context(storage::TypeStorage &ts_, storage::EntityStorage &es_)
      : Context(ts_, es_, std::make_shared<Scope>()) {}

  Context with_names(std::unordered_map<std::string, ast::EntityId> entities) const {
    return {ts, es, std::make_shared<Scope>(std::move(entities), m_scope)};
  }

  std::optional<ast::EntityId> lookup(std::string_view name) const { return m_scope->lookup(name); }

  storage::TypeStorage &ts;
  storage::EntityStorage &es;

private:
  std::shared_ptr<Scope> m_scope;
};

} // namespace parser

#endif

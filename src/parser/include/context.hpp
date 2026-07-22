#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "ast.hpp"
#include "storage/entity_storage.hpp"
#include "storage/type_storage.hpp"
#include <optional>

namespace parser {

struct Scope {
  static std::optional<ast::EntityId> lookup(Scope const *scope, std::string_view name) {
    while (true) {
      if (auto it = scope->m_entities.find(std::string(name)); it != scope->m_entities.end()) {
        return it->second;
      }
      if (not scope->m_parent) {
        return std::nullopt;
      }
      scope = scope->m_parent.get();
    }
  }

  static void capture(storage::EntityStorage const &es, Scope *scope, ast::EntityId entity_id) {
    if (not es.is_binding(entity_id)) {
      return;
    }

    while (true) {
      if (std::ranges::find(scope->m_entities, entity_id, [](auto &e) { return e.second; }) !=
          scope->m_entities.end()) {
        return;
      }
      if (not scope->m_parent) {
        // If this happens, something got screeeewed
        todo();
      }

      // FIX: err check
      scope->m_captures.insert(entity_id);
      scope = scope->m_parent.get();
    }
  }

  Scope() = default;
  Scope(std::unordered_map<std::string, ast::EntityId> entities, std::shared_ptr<Scope> parent)
      : m_entities(entities), m_parent(parent) {}

  std::unordered_set<ast::EntityId> &captures() { return m_captures; }

private:
  std::unordered_map<std::string, ast::EntityId> m_entities;
  std::shared_ptr<Scope> m_parent;
  std::unordered_set<ast::EntityId> m_captures;
};

struct Context {
  Context(storage::TypeStorage &ts_, storage::EntityStorage &es_)
      : Context(ts_, es_, std::make_shared<Scope>(), {}) {}

  // TODO: The keys could be std::string_view.
  Context with_names(std::unordered_map<std::string, ast::EntityId> entities) const {
    return {ts, es, std::make_shared<Scope>(std::move(entities), m_scope), m_type_binding_ids};
  }

  std::optional<ast::EntityId> lookup(std::string_view name) const {
    return Scope::lookup(m_scope.get(), name);
  }

  void capture(ast::EntityId const &result) { return Scope::capture(es, m_scope.get(), result); }
  std::unordered_set<ast::EntityId> const &captures() const { return m_scope->captures(); }

  std::size_t type_binding_index(ast::EntityId type_binding_id) const {
    auto it = std::ranges::find(m_type_binding_ids, type_binding_id);
    if (it == m_type_binding_ids.end()) {
      todo();
    }
    return static_cast<std::size_t>(m_type_binding_ids.end() - it) - 1;
  }

  void push_type_binding(ast::EntityId type_binding_id) {
    m_type_binding_ids.push_back(type_binding_id);
  }
  void pop_type_binding() { m_type_binding_ids.pop_back(); }

  storage::TypeStorage &ts;
  storage::EntityStorage &es;

private:
  Context(storage::TypeStorage &ts_, storage::EntityStorage &es_, std::shared_ptr<Scope> scope,
          std::vector<ast::EntityId> type_binding_ids)
      : ts(ts_), es(es_), m_scope(std::move(scope)),
        m_type_binding_ids(std::move(type_binding_ids)) {}

  std::shared_ptr<Scope> m_scope;
  std::vector<ast::EntityId> m_type_binding_ids;
};

} // namespace parser

#endif

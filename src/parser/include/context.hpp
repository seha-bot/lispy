#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include "ast.hpp"
#include "storage/entity_storage.hpp"
#include "storage/type_storage.hpp"
#include <optional>
#include <utility>

namespace parser {

namespace scope {

struct TypeBinding {
  std::size_t absolute_index;
};
struct TypeFormDefinition {
  ast::EntityId id;
};
struct Binding {
  ast::entity::Binding *binding_ptr;
};
struct ValueDeclaration {
  ast::EntityId id;
};
struct ValueDefinition {
  ast::EntityId id;
};
struct MergedValueDefinition {
  ast::EntityId id;
};

using EntryBase = std::variant<TypeBinding, TypeFormDefinition, Binding, ValueDeclaration,
                               ValueDefinition, MergedValueDefinition>;
struct Entry : EntryBase {
  using EntryBase::variant;
};

// TODO: Move into a separate file.
struct Scope {
  Scope() = default;
  Scope(std::unordered_map<std::string, Entry> bindings, std::shared_ptr<Scope> parent)
      : m_entries(std::move(bindings)), m_parent(std::move(parent)) {}

  std::optional<Entry> lookup(std::string_view name) const {
    Scope const *scope = this;
    while (true) {
      if (auto it = scope->m_entries.find(std::string(name)); it != scope->m_entries.end()) {
        return it->second;
      }
      if (not scope->m_parent) {
        return std::nullopt;
      }
      scope = scope->m_parent.get();
    }
  }

  void capture(ast::entity::Binding const &binding) {
    Scope *scope = this;
    while (true) {
      for (auto &[_, entry] : scope->m_entries) {
        auto *binding_entry = std::get_if<scope::Binding>(&entry);
        if (binding_entry and binding_entry->binding_ptr == &binding) {
          return;
        }
      }
      if (not scope->m_parent) {
        // If this executes, something got screeeewed.
        // This means you're trying to capture a binding which is not in scope.
        std::unreachable();
      }

      // FIX: err check
      scope->m_captures.insert(&binding);
      scope = scope->m_parent.get();
    }
  }

  std::unordered_set<ast::entity::Binding const *> const &captures() const { return m_captures; }

private:
  std::unordered_map<std::string, Entry> m_entries;
  std::shared_ptr<Scope> m_parent;
  std::unordered_set<ast::entity::Binding const *> m_captures;
};

} // namespace scope

struct Context {
  Context(storage::TypeStorage &ts_, storage::EntityStorage &es_, storage::TagStorage &tags_)
      : Context(ts_, es_, tags_, std::make_shared<scope::Scope>(), 0) {}

  // TODO: The keys could be std::string_view.
  Context with_names(std::unordered_map<std::string, scope::Entry> names) const {
    return {ts, es, tags, std::make_shared<scope::Scope>(std::move(names), m_scope),
            m_type_binding_count};
  }

  scope::Scope &scope() const { return *m_scope; }

  std::size_t type_binding_relative_index(std::size_t type_binding_absolute_index) const {
    return m_type_binding_count - 1 - type_binding_absolute_index;
  }
  std::size_t push_type_binding() { return m_type_binding_count++; }

  storage::TypeStorage &ts;
  storage::EntityStorage &es;
  storage::TagStorage &tags;

private:
  Context(storage::TypeStorage &ts_, storage::EntityStorage &es_, storage::TagStorage &tags_,
          std::shared_ptr<scope::Scope> scope, std::size_t type_binding_count)
      : ts(ts_), es(es_), tags(tags_), m_scope(std::move(scope)),
        m_type_binding_count(type_binding_count) {}

  std::shared_ptr<scope::Scope> m_scope;
  // TODO: strongly type this.
  std::size_t m_type_binding_count;
};

} // namespace parser

#endif

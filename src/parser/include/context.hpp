#ifndef CONTEXT_HPP
#define CONTEXT_HPP

#include <cstddef>
#include <memory>
#include <utility>

#include "entity_storage.hpp"
#include "scope.hpp"
#include "storage/type_storage.hpp"
#include "tag_storage.hpp"

namespace parser {

struct Context {
  Context(storage::TypeStorage &ts_, EntityStorage &es_, TagStorage &tags_)
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
  EntityStorage &es;
  TagStorage &tags;

private:
  Context(storage::TypeStorage &ts_, EntityStorage &es_, TagStorage &tags_,
          std::shared_ptr<scope::Scope> scope, std::size_t type_binding_count)
      : ts(ts_), es(es_), tags(tags_), m_scope(std::move(scope)),
        m_type_binding_count(type_binding_count) {}

  std::shared_ptr<scope::Scope> m_scope;
  // TODO: strongly type this.
  std::size_t m_type_binding_count;
};

} // namespace parser

#endif

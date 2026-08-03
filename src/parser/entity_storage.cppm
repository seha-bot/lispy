module;

#include <optional>
#include <utility>
#include <vector>

export module entity_storage;

import ast;
import entity;
import id;
import todo;

export namespace parser {

struct EntityStorage {
  id::EntityId reserve() {
    id::EntityId id{{.value = m_entities.size()}};
    m_entities.push_back(std::nullopt);
    return id;
  }

  void store(id::EntityId id, entity::ModuleEntity<ast::expr::Expr> entity) {
    m_entities[id.value] = std::move(entity);
  }

  id::FormId reserve_form() {
    id::FormId id{{.value = m_forms.size()}};
    m_forms.push_back(std::nullopt);
    return id;
  }

  void store_form(id::FormId id, entity::TypeFormDefinition entity) {
    m_forms[id.value] = std::move(entity);
  }

  std::pair<std::vector<entity::ModuleEntity<ast::expr::Expr>>,
            std::vector<entity::TypeFormDefinition>>
  finalize() && {
    std::vector<entity::ModuleEntity<ast::expr::Expr>> entities;
    entities.reserve(m_entities.size());
    for (auto &entity : m_entities) {
      if (not entity) {
        todo();
      }
      entities.push_back(*std::move(entity));
    }

    std::vector<entity::TypeFormDefinition> forms;
    forms.reserve(m_forms.size());
    for (auto &form : m_forms) {
      if (not form) {
        todo();
      }
      forms.push_back(*std::move(form));
    }
    return std::make_pair(std::move(entities), std::move(forms));
  }

private:
  std::vector<std::optional<entity::ModuleEntity<ast::expr::Expr>>> m_entities;
  std::vector<std::optional<entity::TypeFormDefinition>> m_forms;
};

} // namespace parser

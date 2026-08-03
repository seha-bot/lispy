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

  std::vector<entity::ModuleEntity<ast::expr::Expr>> finalize() && {
    std::vector<entity::ModuleEntity<ast::expr::Expr>> entities;
    entities.reserve(m_entities.size());
    for (auto &entity : m_entities) {
      if (not entity) {
        todo();
      }
      entities.push_back(*std::move(entity));
    }
    return entities;
  }

private:
  std::vector<std::optional<entity::ModuleEntity<ast::expr::Expr>>> m_entities;
};

} // namespace parser

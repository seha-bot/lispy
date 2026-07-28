module;

#include <optional>
#include <utility>
#include <vector>

export module entity_storage;

import ast;
import todo;

export namespace parser {

struct EntityStorage {
  ast::EntityId reserve() {
    ast::EntityId id(m_entities.size());
    m_entities.push_back(std::nullopt);
    return id;
  }

  void store(ast::EntityId id, ast::entity::ModuleEntity entity) {
    m_entities[id.value] = std::move(entity);
  }

  std::vector<ast::entity::ModuleEntity> finalize() && {
    std::vector<ast::entity::ModuleEntity> entities;
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
  std::vector<std::optional<ast::entity::ModuleEntity>> m_entities;
};

} // namespace parser

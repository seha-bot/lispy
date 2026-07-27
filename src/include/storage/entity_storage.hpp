#ifndef ENTITY_STORAGE
#define ENTITY_STORAGE

#include <unordered_map>

#include "ast.hpp"
#include "todo.hpp"

namespace storage {

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

struct TagStorage {
  ast::TagId get_tag(std::string name) {
    ast::TagId const tag_id{m_tags.size()};
    ast::Tag tag{std::move(name)};

    auto [iter, did_insert] = m_tags.insert({std::move(tag), tag_id});
    if (did_insert) {
      iter->second = tag_id;
    }
    return iter->second;
  }

  std::vector<ast::Tag> finalize() && {
    std::vector<ast::Tag> tags(m_tags.size());
    while (not m_tags.empty()) {
      auto handle = m_tags.extract(m_tags.begin());
      tags.push_back(std::move(handle.key()));
    }
    return tags;
  }

private:
  using Hasher = decltype([](ast::Tag const &x) { return std::hash<std::string>{}(x.name); });
  std::unordered_map<ast::Tag, ast::TagId, Hasher> m_tags;
};

} // namespace storage

#endif

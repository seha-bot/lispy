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

  void store(ast::EntityId id, ast::Entity entity) { m_entities[id.value] = std::move(entity); }

  ast::EntityId reserve_store(ast::Entity entity) {
    auto id = reserve();
    store(id, std::move(entity));
    return id;
  }

  std::optional<std::string_view> name_of(ast::EntityId id) const {
    return m_entities[id.value].transform(
        [](ast::Entity const &entity) -> std::string_view { return entity.name(); });
  }

  std::vector<ast::Entity> produce() {
    std::vector<ast::Entity> entities;
    entities.reserve(m_entities.size());
    for (auto &entity : m_entities) {
      if (not entity) {
        todo();
      }
      entities.push_back(*std::move(entity));
    }
    return entities;
  }

  ast::TagId get_tag(std::string name) {
    ast::TagId tag_id{m_tags.size()};
    ast::Tag tag{std::move(name)};

    auto [iter, did_insert] = m_tags.insert({std::move(tag), tag_id});
    if (did_insert) {
      iter->second = tag_id;
    }
    return iter->second;
  }

  ast::TypeFormDefinition *get_if_type_form_definition(ast::EntityId id) {
    auto &entity_opt = m_entities[id.value];
    if (not entity_opt) {
      // TODO: i think this triggers when you try constructing a recursive type
      // which can never be constructed.
      todo();
    }
    return std::get_if<ast::TypeFormDefinition>(&*entity_opt);
  }

  bool is_binding(ast::EntityId id) const {
    auto &entity = m_entities.at(id.value);
    return entity and std::holds_alternative<ast::Binding>(*entity);
  }

private:
  std::vector<std::optional<ast::Entity>> m_entities;
  using Hasher = decltype([](ast::Tag const &x) { return std::hash<std::string>{}(x.name); });
  std::unordered_map<ast::Tag, ast::TagId, Hasher> m_tags;
};

} // namespace storage

#endif

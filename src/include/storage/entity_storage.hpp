#ifndef ENTITY_STORAGE
#define ENTITY_STORAGE

#include <functional>
#include <type_traits>
#include <unordered_map>

#include "ast.hpp"
#include "todo.hpp"

namespace storage {

namespace detail {

template <typename T, typename V> consteval std::size_t variant_type_index() {
  return []<std::size_t... Is>(std::index_sequence<Is...>) {
    std::size_t i = 0, d = 1;
    ((std::is_same_v<T, std::variant_alternative_t<Is, V>> ? d = 0 : i += d) and ...);
    if (i == sizeof...(Is)) {
      throw std::bad_variant_access{};
    }
    return i;
  }(std::make_index_sequence<std::variant_size_v<V>>{});
}

} // namespace detail

struct EntityStorage {
  ast::EntityId reserve(std::size_t entity_type_index) {
    ast::EntityId id(m_entities.size());
    m_entities.push_back(std::nullopt);
    m_entity_type_indexes.push_back(entity_type_index);
    return id;
  }

  void store(ast::EntityId id, ast::Entity entity) { m_entities[id.value] = std::move(entity); }

  template <typename T> ast::EntityId reserve_store(T entity) {
    auto id = reserve(detail::variant_type_index<T, ast::EntityBase>());
    store(id, std::move(entity));
    return id;
  }

  template <typename Fn>
  auto transform(ast::EntityId id, Fn &&fn) const
      -> std::optional<decltype(std::invoke(std::declval<Fn>(),
                                            std::declval<ast::Entity const &>()))> {
    return m_entities.at(id.value).transform(std::forward<Fn>(fn));
  }

  template <typename T> bool holds_alternative(ast::EntityId id) const {
    return m_entity_type_indexes.at(id.value) == detail::variant_type_index<T, ast::EntityBase>();
  }

  std::optional<std::string_view> name_of(ast::EntityId id) const {
    return transform(id,
                     [](ast::Entity const &entity) -> std::string_view { return entity.name(); });
  }

  std::pair<std::vector<ast::Entity>, std::vector<ast::Tag>> produce() {
    std::vector<ast::Entity> entities;
    entities.reserve(m_entities.size());
    for (auto &entity : m_entities) {
      if (not entity) {
        todo();
      }
      entities.push_back(*std::move(entity));
    }

    std::vector<ast::Tag> tags(m_tags.size());
    for (auto &[tag, tag_id] : m_tags) {
      tags[tag_id.id] = std::move(tag);
    }

    return {std::move(entities), std::move(tags)};
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

  bool is_binding(ast::EntityId id) const {
    return transform(id, [](auto &e) { return std::holds_alternative<ast::Binding>(e); })
        .value_or(false);
  }

private:
  std::vector<std::optional<ast::Entity>> m_entities;
  std::vector<std::size_t> m_entity_type_indexes;
  using Hasher = decltype([](ast::Tag const &x) { return std::hash<std::string>{}(x.name); });
  std::unordered_map<ast::Tag, ast::TagId, Hasher> m_tags;
};

} // namespace storage

#endif

#ifndef TYPE_STORAGE_HPP
#define TYPE_STORAGE_HPP

#include <expected>
#include <list>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "todo.hpp"

namespace storage {

struct RepresentativeSets {
  using Hasher = decltype([](ast::TypeId t) { return t.m_id; });
  using Eq = decltype([](ast::TypeId a, ast::TypeId b) { return a.m_id == b.m_id; });

  void directional_merge(ast::TypeId a, ast::TypeId b) {
    // TODO: no need to lookup again, representative impl has the iterator already.
    // Just expose it.
    m_root.find(representative(a))->second = representative(b);
  }

  bool equal(ast::TypeId a, ast::TypeId b) {
    return representative(a).m_id == representative(b).m_id;
  }

  ast::TypeId representative(ast::TypeId a) {
    std::unordered_set<ast::TypeId, Hasher, Eq> seen;
    while (true) {
      if (not seen.insert(a).second) {
        todo();
      }

      auto [it, _] = m_root.insert({a, a});
      auto &a_parent = it->second;
      if (a_parent.m_id == a.m_id) {
        return a;
      } else {
        // TODO: path compression.
        // return a_parent = representative(a_parent);
        a = a_parent;
      }
    }
  }

  std::unordered_map<ast::TypeId, ast::TypeId, Hasher, Eq> m_root;
};

struct TypeStorage {
  TypeStorage() = default;
  TypeStorage(TypeStorage &&) = delete;
  TypeStorage &operator=(TypeStorage &&) = delete;

  // TODO: no need to redundancy checking.
  ast::TypeId make_variable() { return store(ast::TypeVariable{m_variable_id++}); }

  ast::TypeId store(ast::Type type) {
    for (std::size_t i = 0; i < m_types.size(); ++i) {
      if (m_types[i] == type) {
        return ast::TypeId{i, m_rep};
      }
    }
    ast::TypeId id(m_types.size(), m_rep);
    m_types.push_back(std::move(type));
    return id;
  }

  ast::Type const &read(ast::TypeId id) const { return m_types[m_rep.representative(id).m_id]; }

  std::vector<std::pair<ast::LabelId, std::optional<ast::TypeId>>> const &
  get_variants(std::vector<ast::Entity> const &entities, ast::TypeId id) const {
    if (auto *variant = std::get_if<ast::TypeVariant>(&read(id))) {
      return variant->elements;
    }
    if (auto *type = std::get_if<ast::TypeReference>(&read(id))) {
      auto &ent = entities[type->definition.value];
      if (auto *def = std::get_if<ast::TypeFormDefinition>(&ent)) {
        return get_variants(entities, def->type);
      } else {
        todo();
      }
    }
    todo();
  }

  bool merge(ast::TypeId a_id, ast::TypeId b_id) {
    // TODO: calling read more times than needed.
    auto &a = read(a_id);
    auto &b = read(b_id);
    auto *a_arr = std::get_if<ast::TypeArrow>(&a);
    auto *b_arr = std::get_if<ast::TypeArrow>(&b);

    if (m_rep.equal(a_id, b_id)) {
      return true;
    } else if (is_variable(a_id)) {
      m_rep.directional_merge(a_id, b_id);
      return true;
    } else if (is_variable(b_id)) {
      m_rep.directional_merge(b_id, a_id);
      return true;
    } else if (a_arr and b_arr) {
      // TODO: can you create a strong exception guarantee here?
      return merge(a_arr->from, b_arr->from) and merge(a_arr->to, b_arr->to);
    } else {
      todo();
    }
  }

private:
  bool is_variable(ast::TypeId type) const {
    return std::holds_alternative<ast::TypeVariable>(read(type));
  }

  mutable RepresentativeSets m_rep;
  std::vector<ast::Type> m_types;
  std::size_t m_variable_id = 0;
};

struct TypeEnv {
  std::unordered_map<ast::Expr const *, ast::TypeId> type_of;
};

} // namespace storage

#endif

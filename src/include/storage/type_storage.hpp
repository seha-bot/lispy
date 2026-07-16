#ifndef TYPE_STORAGE_HPP
#define TYPE_STORAGE_HPP

#include <algorithm>
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
  using Hasher = decltype([](ast::TypeId t) { return t.value; });
  using Eq = decltype([](ast::TypeId a, ast::TypeId b) { return a.value == b.value; });
  using Map = std::unordered_map<ast::TypeId, ast::TypeId, Hasher, Eq>;

  void directional_merge_unchecked(Map::iterator a, ast::TypeId b) { a->second = b; }

  void directional_merge(ast::TypeId a, ast::TypeId b) {
    representative_iterator(a)->second = representative(b);
  }

  bool equal(ast::TypeId a, ast::TypeId b) { return Eq{}(representative(a), representative(b)); }

  Map::iterator representative_iterator(ast::TypeId a) {
    std::unordered_set<ast::TypeId, Hasher, Eq> seen;
    while (true) {
      if (not seen.insert(a).second) {
        todo();
      }

      auto [it, _] = m_root.insert({a, a});
      auto &a_parent = it->second;
      if (a_parent.value == a.value) {
        return it;
      } else {
        // TODO: path compression.
        // return a_parent = representative(a_parent);
        a = a_parent;
      }
    }
  }

  ast::TypeId representative(ast::TypeId a) { return representative_iterator(a)->first; }

  Map m_root;
};

struct TypeStorage {
  TypeStorage() = default;
  TypeStorage(TypeStorage &&) = delete;
  TypeStorage &operator=(TypeStorage &&) = delete;

  ast::TypeId make_variable() {
    ast::TypeId id{m_types.size()};
    m_types.push_back(ast::TypeVariable{});
    return id;
  }

  ast::TypeId store(ast::Type type) {
    for (std::size_t i = 0; i < m_types.size(); ++i) {
      if (equal(m_types[i], type)) {
        return ast::TypeId{i};
      }
    }
    ast::TypeId id{m_types.size()};
    m_types.push_back(std::move(type));
    return id;
  }

  ast::Type const &read(ast::TypeId id) const { return m_types[m_rep.representative(id).value]; }

  std::string to_string(ast::TypeId id) const {
    return std::to_string(m_rep.representative(id).value);
  }

  // TODO: remove if unused.
  // std::vector<ast::TypeVariant::Element> const &
  // get_variant_elements(std::vector<ast::Entity> const &entities, ast::TypeId id) const {
  //   if (auto *variant = std::get_if<ast::TypeVariant>(&read(id))) {
  //     return variant->elements;
  //   }
  //   if (auto *type = std::get_if<ast::NamedType>(&read(id))) {
  //     auto &ent = entities[type->definition.value];
  //     if (auto *def = std::get_if<ast::TypeFormDefinition>(&ent)) {
  //       return get_variant_elements(entities, def->type);
  //     } else {
  //       todo();
  //     }
  //   }
  //   todo();
  // }

  bool merge(ast::TypeId a_id, ast::TypeId b_id) {
    auto a_rep_it = m_rep.representative_iterator(a_id);
    auto b_rep_it = m_rep.representative_iterator(b_id);
    auto &a = m_types[a_rep_it->first.value];
    auto &b = m_types[b_rep_it->first.value];
    auto *a_arr = std::get_if<ast::TypeArrow>(&a);
    auto *b_arr = std::get_if<ast::TypeArrow>(&b);

    if (RepresentativeSets::Eq{}(a_rep_it->first, b_rep_it->first)) {
      return true;
    } else if (is_variable(a)) {
      m_rep.directional_merge_unchecked(a_rep_it, b_rep_it->first);
      return true;
    } else if (is_variable(b)) {
      m_rep.directional_merge_unchecked(b_rep_it, a_rep_it->first);
      return true;
    } else if (a_arr and b_arr) {
      // TODO: can you create a strong exception guarantee here?
      return merge(a_arr->from, b_arr->from) and merge(a_arr->to, b_arr->to);
    } else {
      todo();
    }
  }

private:
  bool is_variable(ast::Type const &type) const {
    return std::holds_alternative<ast::TypeVariable>(type);
  }

  struct EqualVisitor {
    bool operator()(ast::NamedType const &a, ast::NamedType const &b) {
      return a.definition == b.definition;
    }
    bool operator()(ast::TypeArrow const &a, ast::TypeArrow const &b) {
      return rep.equal(a.from, b.from) and rep.equal(a.to, b.to);
    }
    bool operator()(ast::TypeVariant const &a, ast::TypeVariant const &b) {
      return std::ranges::equal(
          a.elements, b.elements,
          [&](ast::TypeVariant::Element const &e1, ast::TypeVariant::Element const &e2) {
            return e1.tag == e2.tag and rep.equal(e1.type, e2.type);
          });
    }
    bool operator()(ast::TypeStruct const &a, ast::TypeStruct const &b) {
      return std::ranges::equal(
          a.elements, b.elements,
          [&](ast::TypeStruct::Element const &e1, ast::TypeStruct::Element const &e2) {
            return e1.tag == e2.tag and rep.equal(e1.type, e2.type);
          });
    }
    bool operator()(ast::TypeApplication const &a, ast::TypeApplication const &b) {
      return rep.equal(a.function, b.function) and
             std::ranges::equal(a.arguments, b.arguments,
                                [&](ast::TypeId t1, ast::TypeId t2) { return rep.equal(t1, t2); });
    }

    // Exhaustive alternatives for non-equal types.
    bool operator()(ast::NamedType const &, auto &) { return false; }
    bool operator()(ast::TypeArrow const &, auto &) { return false; }
    bool operator()(ast::TypeVariant const &, auto &) { return false; }
    bool operator()(ast::TypeStruct const &, auto &) { return false; }
    bool operator()(ast::TypeApplication const &, auto &) { return false; }
    bool operator()(ast::TypeVariable const &, auto &) { return false; }

    RepresentativeSets &rep;
  };

  bool equal(ast::Type const &a, ast::Type const &b) const {
    return std::visit(EqualVisitor{m_rep}, a, b);
  }

  mutable RepresentativeSets m_rep;
  std::vector<ast::Type> m_types;
};

struct TypeEnv {
  std::unordered_map<ast::Expr const *, ast::TypeId> type_of;
};

} // namespace storage

#endif

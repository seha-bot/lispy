module;

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module type_storage;

import ast;
import entity;
import id;
import tag;
import todo;
import type;

export namespace storage {

struct RepresentativeSets {
  struct Hasher {
    static std::size_t operator()(id::TypeId t) noexcept { return t.value; }
  };
  struct Eq {
    static bool operator()(id::TypeId a, id::TypeId b) noexcept { return a.value == b.value; }
  };
  using Map = std::unordered_map<id::TypeId, id::TypeId, Hasher, Eq>;

  // Merges `a` into `b`.
  void directional_merge_unchecked(Map::iterator a, id::TypeId b) { a->second = b; }

  void directional_merge(id::TypeId a, id::TypeId b) {
    representative_iterator(a)->second = representative(b);
  }

  bool equal(id::TypeId a, id::TypeId b) { return Eq{}(representative(a), representative(b)); }

  Map::iterator representative_iterator(id::TypeId a) {
    std::unordered_set<id::TypeId, Hasher, Eq> seen;
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

  id::TypeId representative(id::TypeId a) { return representative_iterator(a)->first; }

  Map m_root;
};

struct TypeStorage {
  TypeStorage() = default;
  TypeStorage(TypeStorage const &) = delete;
  TypeStorage &operator=(TypeStorage const &) = delete;
  TypeStorage(TypeStorage &&) = delete;
  TypeStorage &operator=(TypeStorage &&) = delete;
  ~TypeStorage() = default;

  id::TypeId make_variable() {
    id::TypeId id{m_types.size()};
    m_types.push_back(type::Variable{});
    return id;
  }

  id::TypeId store(type::Type type) {
    for (std::size_t i = 0; i < m_types.size(); ++i) {
      if (type_equal(m_types[i], type)) {
        return id::TypeId{i};
      }
    }
    id::TypeId id{m_types.size()};
    m_types.push_back(std::move(type));
    return id;
  }

  type::Type const &read(id::TypeId id) const {
    if (id.value == id::TypeId::unit_id.value) {
      static type::Type const unit{type::Struct{}};
      return unit;
    }
    return m_types.at(m_rep.representative(id).value);
  }

  bool equal(id::TypeId a_id, id::TypeId b_id) const { return m_rep.equal(a_id, b_id); }

  /// If this returns false, then the entire TypeStorage is in an invalid state.
  [[nodiscard]] bool merge(id::TypeId a_id, id::TypeId b_id) {
    auto a_rep_it = m_rep.representative_iterator(a_id);
    auto b_rep_it = m_rep.representative_iterator(b_id);
    auto &a = m_types[a_rep_it->first.value];
    auto &b = m_types[b_rep_it->first.value];
    auto *a_arr = std::get_if<type::Arrow>(&a);
    auto *b_arr = std::get_if<type::Arrow>(&b);

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
      return merge(a_arr->from_id, b_arr->from_id) and merge(a_arr->to_id, b_arr->to_id);
    } else {
      return false;
    }
  }

  std::string type_name(std::vector<entity::ModuleEntity<ast::expr::Expr>> const &entities,
                        std::vector<tag::Tag> const &tags, id::TypeId t) const {
    struct Visitor {
      std::string operator()(type::Arrow const &b) {
        return "(" + self.type_name(entities, tags, b.from_id) + ") -> " +
               self.type_name(entities, tags, b.to_id);
      }
      std::string operator()(type::ForAll const &c) {
        return "\\." + self.type_name(entities, tags, c.type_id);
      }
      std::string operator()(type::DeBruijnIndex const &d) { return std::to_string(d.value); }
      std::string operator()(type::Variant const &v) {
        if (v.elements.empty()) {
          return "[]";
        }

        std::string str = "[";
        for (auto &[tag_id, type_id] : v.elements) {
          str += " " + tags[tag_id.value].name.substr(1) + ": " +
                 self.type_name(entities, tags, type_id) + ";";
        }
        return str + " ]";
      }
      std::string operator()(type::Struct const &s) {
        if (s.elements.empty()) {
          return "{}";
        }
        std::string str = "{";
        for (auto &[tag_id, type_id] : s.elements) {
          str += " " + tags[tag_id.value].name.substr(1) + ": " +
                 self.type_name(entities, tags, type_id) + ";";
        }
        return str + " }";
      }
      std::string operator()(type::Application const &) { todo(); }
      std::string operator()(type::Variable const &) {
        return "#" + std::to_string(self.m_rep.representative(t).value);
      }
      std::string operator()(type::NamedTypeReference const &a) {
        return entities[a.definition_id.value].name();
      }

      TypeStorage const &self;
      std::vector<entity::ModuleEntity<ast::expr::Expr>> const &entities;
      std::vector<tag::Tag> const &tags;
      id::TypeId t;
    };
    return std::visit(Visitor{*this, entities, tags, t}, read(t));
  }

private:
  static bool is_variable(type::Type const &type) {
    return std::holds_alternative<type::Variable>(type);
  }

  struct EqualVisitor {
    bool operator()(type::Arrow const &a, type::Arrow const &b) {
      return rep.equal(a.from_id, b.from_id) and rep.equal(a.to_id, b.to_id);
    }
    bool operator()(type::ForAll const &a, type::ForAll const &b) {
      return rep.equal(a.type_id, b.type_id);
    }
    bool operator()(type::DeBruijnIndex const &a, type::DeBruijnIndex const &b) {
      return a.value == b.value;
    }
    bool operator()(type::Variant const &a, type::Variant const &b) {
      return std::ranges::equal(
          a.elements, b.elements, [&](type::Element const &e1, type::Element const &e2) {
            return e1.tag_id == e2.tag_id and rep.equal(e1.type_id, e2.type_id);
          });
    }
    bool operator()(type::Struct const &a, type::Struct const &b) {
      return std::ranges::equal(
          a.elements, b.elements, [&](type::Element const &e1, type::Element const &e2) {
            return e1.tag_id == e2.tag_id and rep.equal(e1.type_id, e2.type_id);
          });
    }
    bool operator()(type::Application const &a, type::Application const &b) {
      return rep.equal(a.function_id, b.function_id) and rep.equal(a.argument_id, b.argument_id);
    }
    bool operator()(type::NamedTypeReference const &a, type::NamedTypeReference const &b) {
      return a.definition_id == b.definition_id;
    }

    // Exhaustive alternatives for non-equal types.
    bool operator()(type::Arrow const &, auto &) { return false; }
    bool operator()(type::ForAll const &, auto &) { return false; }
    bool operator()(type::DeBruijnIndex const &, auto &) { return false; }
    bool operator()(type::Variant const &, auto &) { return false; }
    bool operator()(type::Struct const &, auto &) { return false; }
    bool operator()(type::Application const &, auto &) { return false; }
    bool operator()(type::Variable const &, auto &) { return false; }
    bool operator()(type::NamedTypeReference const &, auto &) { return false; }

    RepresentativeSets &rep;
  };

  bool type_equal(type::Type const &a, type::Type const &b) const {
    return std::visit(EqualVisitor{m_rep}, a, b);
  }

  mutable RepresentativeSets m_rep;
  std::vector<type::Type> m_types;
};

} // namespace storage

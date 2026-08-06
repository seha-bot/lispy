module;

#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module type_storage;

import id;
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

  // Merges `a` into `b`.
  void merge_into(id::TypeId a, id::TypeId b) {
    representative_iterator(a)->second = representative(b);
  }

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
    auto const rep_id = m_rep.representative(id);
    if (rep_id.value == id::TypeId::unit_id.value) {
      static type::Type const unit{type::Struct{}};
      return unit;
    }
    return m_types.at(rep_id.value);
  }

  bool equal(id::TypeId a_id, id::TypeId b_id) const { return m_rep.equal(a_id, b_id); }
  void merge_into(id::VariableId a_id, id::TypeId b_id) { m_rep.merge_into(a_id, b_id); }

  [[nodiscard]] id::TypeId instantiate(id::TypeId type_id) {
    return instantiate_impl(type_id, make_variable(), 0);
  }

private:
  id::TypeId instantiate_impl(id::TypeId type_id, id::TypeId variable_id, std::size_t depth) {
    struct Visitor {
      id::TypeId operator()(type::Arrow const &arr) {
        return ts.store(type::Arrow{
            ts.instantiate_impl(arr.from_id, variable_id, depth),
            ts.instantiate_impl(arr.to_id, variable_id, depth),
        });
      }
      id::TypeId operator()(type::ForAll const &forall) {
        return ts.instantiate_impl(forall.type_id, variable_id, depth + 1);
      }
      id::TypeId operator()(type::DeBruijnIndex const &index) {
        return index.value == depth ? variable_id : type_id;
      }
      id::TypeId operator()(type::Variant const &) { todo(); }
      id::TypeId operator()(type::Struct const &) { todo(); }
      id::TypeId operator()(type::Application const &app) {
        return ts.store(type::Application{
            ts.instantiate_impl(app.function_id, variable_id, depth),
            ts.instantiate_impl(app.argument_id, variable_id, depth),
        });
      }
      id::TypeId operator()(type::Variable const &) { return type_id; }
      id::TypeId operator()(type::NamedTypeReference const &) { return type_id; }

      TypeStorage &ts;
      id::TypeId type_id;
      id::TypeId variable_id;
      std::size_t depth;
    };
    return std::visit(Visitor{*this, type_id, variable_id, depth}, read(type_id));
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

  // FIX: MAKE PRIVATE!
public:
  mutable RepresentativeSets m_rep;
  std::vector<type::Type> m_types;
};

} // namespace storage

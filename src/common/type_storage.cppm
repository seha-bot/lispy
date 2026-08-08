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
private:
  struct Hasher {
    static std::size_t operator()(id::TypeId t) noexcept { return t.value; }
  };
  struct Eq {
    static bool operator()(id::TypeId a, id::TypeId b) noexcept { return a.value == b.value; }
  };
  using Map = std::unordered_map<id::TypeId, id::TypeId, Hasher, Eq>;

public:
  id::TypeId representative(id::TypeId a) { return representative_iterator(a)->first; }

  // Merges `a` into `b`.
  void merge_into(id::TypeId a, id::TypeId b) {
    representative_iterator(a)->second = representative(b);
  }

private:
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

  Map m_root;
};

/// Its only purpose is to store your types.
/// NOTE: Uniqueness of stored types is not guaranteed.
/// assert(ts.equal(a, b));
/// assert(&ts.read(a) == &ts.read(b)); // Might not pass.
struct TypeStorage {
  TypeStorage() = default;
  TypeStorage(TypeStorage const &) = delete;
  TypeStorage &operator=(TypeStorage const &) = delete;
  TypeStorage(TypeStorage &&) = delete;
  TypeStorage &operator=(TypeStorage &&) = delete;
  ~TypeStorage() = default;

  // TODO: In order to reduce the amount of types stored, you could keep a vector
  // which records which ids are variables so you may recycle them while typechecking the next
  // definition.
  [[nodiscard]] id::VariableId make_variable() {
    id::TypeId id{m_types.size()};
    m_types.push_back(type::Variable{});
    return id::VariableId{id};
  }

  [[nodiscard]] id::TypeId store(type::Type type) {
    for (std::size_t i = 0; i < m_types.size(); ++i) {
      if (type_equal(m_types[i], type)) {
        return id::TypeId{i};
      }
    }
    id::TypeId id{m_types.size()};
    m_types.push_back(std::move(type));
    return id;
  }

  [[nodiscard]] type::Type const &read(id::TypeId id) const {
    return read_exact(m_rep.representative(id));
  }

  [[nodiscard]] bool equal(id::TypeId a_id, id::TypeId b_id) const {
    auto const a_rep_id = m_rep.representative(a_id);
    auto const b_rep_id = m_rep.representative(b_id);
    if (a_rep_id.value == b_rep_id.value) {
      return true;
    }
    return type_equal(read_exact(a_rep_id), read_exact(b_rep_id));
  }

  void merge_into(id::VariableId a_id, id::TypeId b_id) { m_rep.merge_into(a_id, b_id); }

  /// Replaces all DeBruijn indices pointing to the current root of type_id with subst_id.
  [[nodiscard]] id::TypeId instantiate(id::TypeId type_id, id::TypeId subst_id) {
    return instantiate_impl(type_id, subst_id, 0);
  }

private:
  id::TypeId instantiate_impl(id::TypeId type_id, id::TypeId subst_id, std::size_t depth) {
    struct Visitor {
      id::TypeId operator()(type::Arrow const &arr) {
        return ts.store(type::Arrow{
            ts.instantiate_impl(arr.from_id, subst_id, depth),
            ts.instantiate_impl(arr.to_id, subst_id, depth),
        });
      }
      id::TypeId operator()(type::ForAll const &forall) {
        return ts.instantiate_impl(forall.type_id, subst_id, depth + 1);
      }
      id::TypeId operator()(type::DeBruijnIndex const &index) {
        return index.value == depth ? subst_id : type_id;
      }
      id::TypeId operator()(type::Variant const &v) {
        std::vector<type::Element> elements;
        elements.reserve(v.elements.size());
        for (auto &e : v.elements) {
          elements.push_back({
              .tag_id = e.tag_id,
              .type_id = ts.instantiate_impl(e.type_id, subst_id, depth),
          });
        }
        return ts.store(type::Variant{elements});
      }
      id::TypeId operator()(type::Struct const &s) {
        std::vector<type::Element> elements;
        elements.reserve(s.elements.size());
        for (auto &e : s.elements) {
          elements.push_back({
              .tag_id = e.tag_id,
              .type_id = ts.instantiate_impl(e.type_id, subst_id, depth),
          });
        }
        return ts.store(type::Struct{elements});
      }
      id::TypeId operator()(type::Application const &app) {
        std::vector<id::TypeId> instantiated_argument_ids;
        for (auto &id : app.argument_ids) {
          instantiated_argument_ids.push_back(ts.instantiate_impl(id, subst_id, depth));
        }
        return ts.store(type::Application{app.definition_id, std::move(instantiated_argument_ids)});
      }
      id::TypeId operator()(type::Variable const &) { return type_id; }
      id::TypeId operator()(type::NamedTypeReference const &) { return type_id; }

      TypeStorage &ts;
      id::TypeId type_id;
      id::TypeId subst_id;
      std::size_t depth;
    };
    return std::visit(Visitor{*this, type_id, subst_id, depth}, read(type_id));
  }

  struct EqualVisitor {
    bool operator()(type::Arrow const &a, type::Arrow const &b) {
      return ts.equal(a.from_id, b.from_id) and ts.equal(a.to_id, b.to_id);
    }
    bool operator()(type::ForAll const &a, type::ForAll const &b) {
      return ts.equal(a.type_id, b.type_id);
    }
    bool operator()(type::DeBruijnIndex const &a, type::DeBruijnIndex const &b) {
      return a.value == b.value;
    }
    bool operator()(type::Variant const &a, type::Variant const &b) {
      return std::ranges::equal(
          a.elements, b.elements, [&](type::Element const &e1, type::Element const &e2) {
            return e1.tag_id == e2.tag_id and ts.equal(e1.type_id, e2.type_id);
          });
    }
    bool operator()(type::Struct const &a, type::Struct const &b) {
      return std::ranges::equal(
          a.elements, b.elements, [&](type::Element const &e1, type::Element const &e2) {
            return e1.tag_id == e2.tag_id and ts.equal(e1.type_id, e2.type_id);
          });
    }
    bool operator()(type::Application const &a, type::Application const &b) {
      return a.definition_id == b.definition_id and
             std::ranges::equal(
                 a.argument_ids, b.argument_ids,
                 [&](id::TypeId a_id, id::TypeId b_id) { return ts.equal(a_id, b_id); });
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

    TypeStorage const &ts;
  };

  bool type_equal(type::Type const &a, type::Type const &b) const {
    return std::visit(EqualVisitor{*this}, a, b);
  }

  type::Type const &read_exact(id::TypeId id) const {
    if (id.value == id::TypeId::unit_id.value) {
      static type::Type const unit{type::Struct{}};
      return unit;
    }
    return m_types.at(id.value);
  }

  // FIX: MAKE PRIVATE!
public:
  mutable RepresentativeSets m_rep;
  std::vector<type::Type> m_types;
};

} // namespace storage

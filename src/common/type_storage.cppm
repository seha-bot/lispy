module;

#include <algorithm>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

export module type_storage;

import ast;
import todo;

export namespace constraint {

struct SubtypeOf {
  // a <= b
  ast::TypeId a_id, b_id;
};

using ConstraintBase = std::variant<SubtypeOf>;

struct Constraint : ConstraintBase {
  using ConstraintBase::variant;
};

} // namespace constraint

export namespace storage {

struct RepresentativeSets {
  struct Hasher {
    static std::size_t operator()(ast::TypeId t) noexcept { return t.value; }
  };
  struct Eq {
    static bool operator()(ast::TypeId a, ast::TypeId b) noexcept { return a.value == b.value; }
  };
  using Map = std::unordered_map<ast::TypeId, ast::TypeId, Hasher, Eq>;

  // Merges `a` into `b`.
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
  TypeStorage(TypeStorage const &) = delete;
  TypeStorage &operator=(TypeStorage const &) = delete;
  TypeStorage(TypeStorage &&) = delete;
  TypeStorage &operator=(TypeStorage &&) = delete;
  ~TypeStorage() = default;

  ast::TypeId make_variable() {
    ast::TypeId id{m_types.size()};
    m_types.push_back(ast::type::Variable{});
    return id;
  }

  ast::TypeId store(ast::type::Type type) {
    for (std::size_t i = 0; i < m_types.size(); ++i) {
      if (equal(m_types[i], type)) {
        return ast::TypeId{i};
      }
    }
    ast::TypeId id{m_types.size()};
    m_types.push_back(std::move(type));
    return id;
  }

  ast::type::Type const &read(ast::TypeId id) const {
    if (id.value == ast::TypeId::unit_id.value) {
      static ast::type::Type const unit{ast::type::Struct{}};
      return unit;
    }
    return m_types.at(m_rep.representative(id).value);
  }

  std::string to_string(ast::TypeId id) const {
    return std::to_string(m_rep.representative(id).value);
  }

  /// If this returns false, then the entire TypeStorage is in an invalid state.
  bool merge_(ast::TypeId a_id, ast::TypeId b_id) {
    auto a_rep_it = m_rep.representative_iterator(a_id);
    auto b_rep_it = m_rep.representative_iterator(b_id);
    auto &a = m_types[a_rep_it->first.value];
    auto &b = m_types[b_rep_it->first.value];
    auto *a_arr = std::get_if<ast::type::Arrow>(&a.unnamed_part());
    auto *b_arr = std::get_if<ast::type::Arrow>(&b.unnamed_part());

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
      return merge_(a_arr->from_id, b_arr->from_id) and merge_(a_arr->to_id, b_arr->to_id);
    } else {
      return false;
    }
  }

  void add_constraint(constraint::Constraint) { todo(); }

  bool is_in(std::vector<ast::type::Element> const &a,
             std::vector<ast::type::Element> const &b) const {
    return std::ranges::all_of(a, [&](auto &e1) {
      return std::ranges::any_of(b, [&](auto &e2) {
        return e1.tag_id == e2.tag_id and m_rep.equal(e1.type_id, e2.type_id);
      });
    });
  }

  ast::TypeId instantiate_impl(ast::TypeId type_id, ast::TypeId variable_id, std::size_t depth) {
    struct Visitor {
      ast::TypeId operator()(ast::type::Arrow const &arr) {
        return self.store(ast::type::Arrow{
            self.instantiate_impl(arr.from_id, variable_id, depth),
            self.instantiate_impl(arr.to_id, variable_id, depth),
        });
      }
      ast::TypeId operator()(ast::type::ForAll const &forall) {
        return self.instantiate_impl(forall.type_id, variable_id, depth + 1);
      }
      ast::TypeId operator()(ast::type::DeBruijnIndex const &index) {
        return index.value == depth ? variable_id : type_id;
      }
      ast::TypeId operator()(ast::type::Variant const &) { todo(); }
      ast::TypeId operator()(ast::type::Struct const &) { todo(); }
      ast::TypeId operator()(ast::type::Application const &app) {
        return self.store(ast::type::Application{
            self.instantiate_impl(app.function_id, variable_id, depth),
            self.instantiate_impl(app.argument_id, variable_id, depth),
        });
      }
      ast::TypeId operator()(ast::type::Variable const &) { return type_id; }
      ast::TypeId operator()(ast::type::NamedTypeReference const &) { return type_id; }

      TypeStorage &self;
      ast::TypeId type_id;
      ast::TypeId variable_id;
      std::size_t depth;
    };
    return std::visit(Visitor{*this, type_id, variable_id, depth}, read(type_id).unnamed_part());
  }

  ast::TypeId instantiate(ast::TypeId type_id) {
    return instantiate_impl(type_id, make_variable(), 0);
  }

private:
  bool is_variable(ast::type::Type const &type) const {
    return std::holds_alternative<ast::type::Variable>(type.unnamed_part());
  }

  struct EqualVisitor {
    bool operator()(ast::type::Arrow const &a, ast::type::Arrow const &b) {
      return rep.equal(a.from_id, b.from_id) and rep.equal(a.to_id, b.to_id);
    }
    bool operator()(ast::type::ForAll const &a, ast::type::ForAll const &b) {
      return rep.equal(a.type_id, b.type_id);
    }
    bool operator()(ast::type::DeBruijnIndex const &a, ast::type::DeBruijnIndex const &b) {
      return a.value == b.value;
    }
    bool operator()(ast::type::Variant const &a, ast::type::Variant const &b) {
      return std::ranges::equal(
          a.elements, b.elements, [&](ast::type::Element const &e1, ast::type::Element const &e2) {
            return e1.tag_id == e2.tag_id and rep.equal(e1.type_id, e2.type_id);
          });
    }
    bool operator()(ast::type::Struct const &a, ast::type::Struct const &b) {
      return std::ranges::equal(
          a.elements, b.elements, [&](ast::type::Element const &e1, ast::type::Element const &e2) {
            return e1.tag_id == e2.tag_id and rep.equal(e1.type_id, e2.type_id);
          });
    }
    bool operator()(ast::type::Application const &a, ast::type::Application const &b) {
      return rep.equal(a.function_id, b.function_id) and rep.equal(a.argument_id, b.argument_id);
    }
    bool operator()(ast::type::NamedTypeReference const &a,
                    ast::type::NamedTypeReference const &b) {
      return a.definition_id == b.definition_id;
    }

    // Exhaustive alternatives for non-equal types.
    bool operator()(ast::type::Arrow const &, auto &) { return false; }
    bool operator()(ast::type::ForAll const &, auto &) { return false; }
    bool operator()(ast::type::DeBruijnIndex const &, auto &) { return false; }
    bool operator()(ast::type::Variant const &, auto &) { return false; }
    bool operator()(ast::type::Struct const &, auto &) { return false; }
    bool operator()(ast::type::Application const &, auto &) { return false; }
    bool operator()(ast::type::Variable const &, auto &) { return false; }
    bool operator()(ast::type::NamedTypeReference const &, auto &) { return false; }

    RepresentativeSets &rep;
  };

  bool equal(ast::type::Type const &a, ast::type::Type const &b) const {
    return a.definition() == b.definition() and
           std::visit(EqualVisitor{m_rep}, a.unnamed_part(), b.unnamed_part());
  }

  mutable RepresentativeSets m_rep;
  std::vector<ast::type::Type> m_types;
};

struct TypeEnv {
  std::unordered_map<ast::expr::Expr const *, ast::TypeId> type_of;
};

} // namespace storage

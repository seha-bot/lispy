module;

#include <algorithm>
#include <deque>
#include <iostream>
#include <variant>
#include <vector>

export module constraint;

import ast;
import entity;
import id;
import tag;
import todo;
import type;
import type_storage;

namespace constraint {

export struct SubtypeOf {
  // a <= b
  id::TypeId a_id, b_id;

  bool equal(storage::TypeStorage const &ts, SubtypeOf const &that) {
    return ts.equal(a_id, that.a_id) and ts.equal(b_id, that.b_id);
  }

  SubtypeOf flip() const { return {b_id, a_id}; }
};

namespace {

// using ConstraintBase = std::variant<SubtypeOf>;
//
// struct Constraint : ConstraintBase {
//   using ConstraintBase::ConstraintBase;
// };

using Constraint = SubtypeOf;

struct SubtypeOfRule {
  void operator()(type::Arrow const &a, type::Arrow const &b) {
    constraints.push_back(SubtypeOf{a.from_id, b.from_id});
    constraints.push_back(SubtypeOf{a.to_id, b.to_id});
  }
  void operator()(type::ForAll const &, type::ForAll const &) { todo(); }
  void operator()(type::DeBruijnIndex const &, type::DeBruijnIndex const &) { todo(); }
  void operator()(type::Variant const &a, type::Variant const &b) {
    for (auto &e1 : a.elements) {
      auto it = std::ranges::find(b.elements, e1.tag_id, &type::Element::tag_id);
      if (it == b.elements.end()) {
        todo();
      }
      constraints.push_back(SubtypeOf{e1.type_id, it->type_id});
    }
  }
  void operator()(type::Struct const &a, type::Struct const &b) {
    for (auto &e2 : b.elements) {
      auto it = std::ranges::find(a.elements, e2.tag_id, &type::Element::tag_id);
      if (it == a.elements.end()) {
        todo();
      }
      constraints.push_back(SubtypeOf{it->type_id, e2.type_id});
    }
  }
  void operator()(type::Application const &, type::Application const &) { todo(); }
  void operator()(type::Variable const &, type::Variable const &) { todo(); }
  void operator()(type::NamedTypeReference const &a, type::NamedTypeReference const &b) {
    if (a.definition_id != b.definition_id) {
      todo();
    }
  }

  void operator()(type::Variant const &, type::NamedTypeReference const &b) {
    constraints.push_back(SubtypeOf{
        a_id,
        std::get<entity::TypeFormDefinition>(entities[b.definition_id.value]).type,
    });
  }

  void operator()(type::Variable const &, type::NamedTypeReference const &) {
    constraints.push_back(SubtypeOf{a_id, b_id});
  }
  void operator()(type::NamedTypeReference const &, type::Variable const &) {
    constraints.push_back(SubtypeOf{a_id, b_id});
  }

  // Exhaustive branches.
  void operator()(type::Arrow const &, auto &&) { todo(); }
  void operator()(type::ForAll const &, auto &&) { todo(); }
  void operator()(type::DeBruijnIndex const &, auto &&) { todo(); }
  void operator()(type::Variant const &, auto &&) { todo(); }
  void operator()(type::Struct const &, auto &&) { todo(); }
  void operator()(type::Application const &, auto &&) { todo(); }
  void operator()(type::Variable const &, auto &&) { todo(); }
  void operator()(type::NamedTypeReference const &, auto &&) { todo(); }
  void operator()(auto &&, auto &&) { todo(); }

  std::vector<entity::ModuleEntity<ast::expr::Expr>> const &entities;
  storage::TypeStorage const &ts;
  std::deque<Constraint> &constraints;
  id::TypeId a_id;
  id::TypeId b_id;
};

} // namespace

export struct Solver {
  void add_constraint(Constraint c) { m_constraints.push_back(c); }

  void solve(std::vector<entity::ModuleEntity<ast::expr::Expr>> const &entities,
             std::vector<tag::Tag> const &tags, storage::TypeStorage &ts) {
  again:
    if (m_constraints.empty()) {
      std::cout << "Done?\n";
      todo();
    }

    auto old_constraints = m_constraints;
    decltype(old_constraints) new_constraints;
    while (not m_constraints.empty()) {
      auto c = m_constraints.front();
      m_constraints.pop_front();

      std::cout << ts.type_name(entities, tags, c.a_id)
                << " <= " << ts.type_name(entities, tags, c.b_id) << '\n';

      std::visit(SubtypeOfRule{entities, ts, new_constraints, c.a_id, c.b_id}, ts.read(c.a_id),
                 ts.read(c.b_id));
    }

    for (std::size_t i = 0; i < new_constraints.size(); ++i) {
      for (std::size_t j = i + 1; j < new_constraints.size(); ++j) {
        if (new_constraints[i].equal(ts, new_constraints[j])) {
          std::cout << "Erasing (duplicate): "
                    << ts.type_name(entities, tags, new_constraints[i].a_id)
                    << " <= " << ts.type_name(entities, tags, new_constraints[i].b_id) << '\n';
          new_constraints.erase(new_constraints.begin() + static_cast<std::ptrdiff_t>(j));
          --j;
        }
      }
    }

    if (not std::ranges::equal(new_constraints, old_constraints,
                               [&](auto &a, auto &b) { return a.equal(ts, b); })) {
      m_constraints = std::move(new_constraints);
      std::cout << "Again...\n";
      goto again;
    }

    for (std::size_t i = 0; i < new_constraints.size(); ++i) {
      auto *var = std::get_if<type::Variable>(&ts.read(new_constraints[i].a_id));
      if (var) {
        // TODO: You could make merge know that a_id is a variable, so that the merge has a 100%
        // success rate.
        std::cout << "Merging " << ts.type_name(entities, tags, new_constraints[i].a_id) << " and "
                  << ts.type_name(entities, tags, new_constraints[i].b_id) << '\n';
        if (not ts.merge(new_constraints[i].a_id, new_constraints[i].b_id)) {
          todo();
        }
      } else {
        todo();
      }
    }

    goto again;
  }

  std::deque<Constraint> m_constraints;
};

} // namespace constraint

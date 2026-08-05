module;

#include <algorithm>
#include <deque>
#include <functional>
#include <ostream>
#include <variant>
#include <vector>

export module constraint;

import entity;
import id;
import todo;
import type;
import type_storage;

namespace constraint {

export struct SubtypeOf {
  // Represents the relation a <= b.
  // 1. For every type a, a <= a is true.
  // 2. If a <= b and b <= c, then a <= c is true.
  // 3. If a <= b and b <= a, then a and b are the same type.
  id::TypeId a_id, b_id;

  bool equal(storage::TypeStorage const &ts, SubtypeOf const &that) {
    return ts.equal(a_id, that.a_id) and ts.equal(b_id, that.b_id);
  }

  SubtypeOf flip() const { return {b_id, a_id}; }

  void log(std::ostream &os, auto log) const {
    log(os, a_id);
    os << " <= ";
    log(os, b_id);
  }
};

namespace {

struct SubtypeOfRule {
  template <typename A, typename B> void operator()(A const &a, B const &b) {
    if constexpr (std::same_as<A, type::Variable> or std::same_as<B, type::Variable>) {
      constraints.push_back(SubtypeOf{a_id, b_id});
    } else if constexpr (std::same_as<A, type::Arrow> and std::same_as<B, type::Arrow>) {
      constraints.push_back(SubtypeOf{a.from_id, b.from_id});
      constraints.push_back(SubtypeOf{a.to_id, b.to_id});
    } else if constexpr (std::same_as<A, type::ForAll> and std::same_as<B, type::ForAll>) {
      constraints.push_back(SubtypeOf{a.type_id, b.type_id});
    } else if constexpr (std::same_as<A, type::DeBruijnIndex> and
                         std::same_as<B, type::DeBruijnIndex>) {
      if (a.value != b.value) {
        todo();
      }
    } else if constexpr (std::same_as<A, type::ForAll> and std::same_as<B, type::Arrow>) {
      constraints.push_back(SubtypeOf{ts.instantiate(a.type_id), b_id});
    } else if constexpr (std::same_as<A, type::Variant> and std::same_as<B, type::Variant>) {
      for (auto &e1 : a.elements) {
        auto it = std::ranges::find(b.elements, e1.tag_id, &type::Element::tag_id);
        if (it == b.elements.end()) {
          todo();
        }
        constraints.push_back(SubtypeOf{e1.type_id, it->type_id});
      }
    } else if constexpr (std::same_as<A, type::Struct> and std::same_as<B, type::Struct>) {
      for (auto &e2 : b.elements) {
        auto it = std::ranges::find(a.elements, e2.tag_id, &type::Element::tag_id);
        if (it == a.elements.end()) {
          todo();
        }
        constraints.push_back(SubtypeOf{it->type_id, e2.type_id});
      }
    } else if constexpr (std::same_as<A, type::NamedTypeReference> and
                         std::same_as<B, type::NamedTypeReference>) {
      if (a.definition_id != b.definition_id) {
        todo();
      }
    } else if constexpr (std::same_as<B, type::NamedTypeReference>) {
      constraints.push_back(SubtypeOf{a_id, forms[b.definition_id.value].type});
    } else {
      // static_assert(false);
      todo();
    }
  }

  std::vector<entity::TypeFormDefinition> const &forms;
  storage::TypeStorage &ts;
  std::deque<SubtypeOf> &constraints;
  id::TypeId a_id;
  id::TypeId b_id;
};

} // namespace

export struct Solver {
  void add_constraint(SubtypeOf c) { m_constraints.push_back(c); }

  void solve(std::ostream &os, std::function<void(std::ostream &os, id::TypeId)> log,
             std::vector<entity::TypeFormDefinition> const &forms, storage::TypeStorage &ts) {
    while (not m_constraints.empty()) {
      for (std::size_t i = 0; i < m_constraints.size(); ++i) {
        for (std::size_t j = i + 1; j < m_constraints.size(); ++j) {
          if (m_constraints[i].equal(ts, m_constraints[j])) {
            os << "Erasing (duplicate): ";
            m_constraints[i].log(os, log);
            os << '\n';
            m_constraints.erase(m_constraints.begin() + static_cast<std::ptrdiff_t>(j));
            --j;
          } else if (m_constraints[i].equal(ts, m_constraints[j].flip())) {
            os << "Constraints produced equal types: ";
            m_constraints[i].log(os, log);
            os << " and ";
            m_constraints[j].log(os, log);
            os << '\n';
            if (not ts.merge(m_constraints[i].a_id, m_constraints[i].b_id)) {
              todo();
            }
            m_constraints.erase(m_constraints.begin() + static_cast<std::ptrdiff_t>(j));
            m_constraints.erase(m_constraints.begin() + static_cast<std::ptrdiff_t>(i));
            --i;
            break;
          }
        }
      }

      {
        auto const old_constraints = m_constraints;
        auto constraints = std::move(m_constraints);
        m_constraints.clear();

        while (not constraints.empty()) {
          auto c = constraints.front();
          constraints.pop_front();

          c.log(os, log);
          os << '\n';

          std::visit(SubtypeOfRule{forms, ts, m_constraints, c.a_id, c.b_id}, ts.read(c.a_id),
                     ts.read(c.b_id));
        }

        if (not std::ranges::equal(m_constraints, old_constraints,
                                   [&](auto &a, auto &b) { return a.equal(ts, b); })) {
          os << "Again...\n";
          continue;
        }
      }

      for (std::size_t i = 0; i < m_constraints.size(); ++i) {
        auto *var = std::get_if<type::Variable>(&ts.read(m_constraints[i].a_id));
        if (var) {
          os << "Merging ";
          log(os, m_constraints[i].a_id);
          os << " and ";
          log(os, m_constraints[i].b_id);
          os << '\n';
          // TODO: You could make merge know that a_id is a variable, so that the merge has a 100%
          // success rate.
          if (not ts.merge(m_constraints[i].a_id, m_constraints[i].b_id)) {
            todo();
          }
        } else {
          todo();
        }
      }
    }
  }

  std::deque<SubtypeOf> m_constraints;
};

} // namespace constraint

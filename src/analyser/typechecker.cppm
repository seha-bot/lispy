module;

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <expected>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

export module typechecker;

import ast;
import constraint;
import entity;
import id;
import resolved;
import tag;
import todo;
import type;
import type_storage;
import typed_expr;

export struct Error {
  friend std::ostream &operator<<(std::ostream &os, Error) { return os; }
};

namespace {

using TypedEntityRefBase = std::variant<                    //
    entity::ValueDeclaration const *,                       //
    entity::ValueDefinition<ast::expr::Expr> const *,       //
    entity::MergedValueDefinition<ast::expr::Expr> const *, //
    entity::Binding const *                                 //
    >;

struct TypedEntityRef : TypedEntityRefBase {
  using TypedEntityRefBase::TypedEntityRefBase;
};

} // namespace

template <> struct std::hash<TypedEntityRef> {
  std::size_t operator()(TypedEntityRef const &e) const noexcept {
    return std::visit([](auto p) { return std::hash<decltype(p)>{}(p); }, e);
  }
};

namespace {

// TODO: There is a class TypeEnv in storage which is very similar to this.
struct Env {
  std::unordered_map<TypedEntityRef, id::TypeId> type_of_entity;
};

struct Context {
  storage::TypeStorage &ts;
  constraint::Solver &solver;
  std::vector<entity::ModuleEntity<ast::expr::Expr>> const &entities;
  std::vector<tag::Tag> const &tags;
  Env env;

  entity::ModuleEntity<ast::expr::Expr> const &entity(id::EntityId e) const {
    return entities[e.value];
  }
  tag::Tag const &tag(id::TagId e) const { return tags[e.value]; }
};

std::expected<id::TypeId, Error> get_entity_type(Context &ctx, TypedEntityRef entity_ref) noexcept;

std::expected<typed_expr::Expr, Error> get_type(Context &ctx, ast::expr::Expr expr) noexcept {
  struct Visitor {
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::Application call) {
      auto function = get_type(ctx, std::move(*call.function));
      if (not function) {
        todo();
      }
      auto argument = get_type(ctx, std::move(*call.argument));
      if (not argument) {
        todo();
      }

      auto type_id = ctx.ts.make_variable();
      ctx.solver.add_constraint(constraint::SubtypeOf{
          ctx.ts.store(type::Arrow{argument->type_id(), type_id}),
          function->type_id(),
      });
      return typed_expr::Application{
          {type_id},
          std::make_unique<typed_expr::Expr>(*std::move(function)),
          std::make_unique<typed_expr::Expr>(*std::move(argument)),
      };
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::Case) {
      todo();
      // auto scrutinee_type = get_type(ctx, *case_.scrutinee);
      // if (not scrutinee_type) {
      //   todo();
      // }
      //
      // std::optional<id::TypeId> common_arm_type;
      // for (auto &[pattern, arm] : case_.choices) {
      //   // FIX: pattern binding should be checked against the declared type.
      //   pattern;
      //
      //   auto arm_type = get_type(ctx, arm);
      //   if (not arm_type) {
      //     todo();
      //   }
      //
      //   if (common_arm_type) {
      //     auto const did_merge = ctx.ts.merge(*common_arm_type, *arm_type);
      //     if (not did_merge) {
      //       todo();
      //     }
      //   } else {
      //     common_arm_type = *arm_type;
      //   }
      // }
      //
      // if (not common_arm_type) {
      //   todo();
      // }
      // return *common_arm_type;
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::Variant variant) {
      if (variant.value) {
        auto value = get_type(ctx, std::move(**variant.value));
        if (not value) {
          todo();
        }
        auto const type_id = ctx.ts.store(type::Variant{std::vector{type::Element{
            .tag_id = variant.tag_id,
            .type_id = value->type_id(),
        }}});
        return typed_expr::Variant{
            {type_id},
            variant.tag_id,
            std::make_unique<typed_expr::Expr>(*std::move(value)),
        };
      } else {
        auto const type_id = ctx.ts.store(type::Variant{std::vector{type::Element{
            .tag_id = variant.tag_id,
            .type_id = id::TypeId::unit_id,
        }}});

        return typed_expr::Variant{
            {type_id},
            variant.tag_id,
            std::nullopt,
        };
      }
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::Pack pack) {
      std::vector<type::Element> elements;
      std::vector<typed_expr::TaggedValue> tagged_values;
      for (auto &[tag_id, value] : pack.tagged_values) {
        auto typed_value = get_type(ctx, std::move(*value));
        if (not typed_value) {
          todo();
        }

        elements.push_back({
            .tag_id = tag_id,
            .type_id = typed_value->type_id(),
        });
        tagged_values.push_back({
            .tag_id = tag_id,
            .value = std::make_unique<typed_expr::Expr>(*std::move(typed_value)),
        });
      }

      std::ranges::sort(elements, {}, [](auto &e) { return e.tag_id; });
      auto const type_id = ctx.ts.store(type::Struct{std::move(elements)});
      return typed_expr::Pack{{type_id}, std::move(tagged_values)};
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::Lambda lambda) {
      auto binding_type_id = get_entity_type(ctx, lambda.binding.get());
      if (not binding_type_id) {
        todo();
      }
      auto body = get_type(ctx, std::move(*lambda.body));
      if (not body) {
        todo();
      }
      // TODO: You might not have to check for duplicates in store if binding_type_id represents a
      // fresh variable.
      auto const type_id = ctx.ts.store(type::Arrow{*binding_type_id, body->type_id()});
      return typed_expr::Lambda{
          {type_id},
          std::move(lambda.captures),
          std::move(lambda.binding),
          std::make_unique<typed_expr::Expr>(*std::move(body)),
      };
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::TVLambda tv_lambda) {
      // TODO: Figure out kind inference.
      // auto binding_type = get_entity_kind(ctx, tv_lambda.binding_id);
      // if (not binding_type) {
      //   todo();
      // }

      auto body = get_type(ctx, std::move(*tv_lambda.body));
      if (not body) {
        todo();
      }
      auto const type_id = ctx.ts.store(type::ForAll{body->type_id()});
      return typed_expr::TVLambda{{type_id}, std::make_unique<typed_expr::Expr>(*std::move(body))};
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::ValueReference value_ref) {
      auto const type_id = [&] {
        auto &entity = ctx.entity(value_ref.value_entity_id);
        auto *dec = std::get_if<entity::ValueDeclaration>(&entity);
        auto *def = std::get_if<entity::ValueDefinition<ast::expr::Expr>>(&entity);
        auto *mer = std::get_if<entity::MergedValueDefinition<ast::expr::Expr>>(&entity);
        if (dec) {
          return ctx.env.type_of_entity[dec];
        } else if (def) {
          return ctx.env.type_of_entity[def];
        } else if (mer) {
          return ctx.env.type_of_entity[mer];
        } else {
          std::unreachable();
        }
      }();
      return typed_expr::ValueReference{{type_id}, value_ref.value_entity_id};
    }
    std::expected<typed_expr::Expr, Error> operator()(ast::expr::BindingReference binding_ref) {
      return typed_expr::BindingReference{
          {ctx.env.type_of_entity[&binding_ref.binding.get()]},
          binding_ref.binding,
      };
    }

    Context &ctx;
  };
  return std::visit(Visitor{ctx}, std::move(expr));
}

std::expected<id::TypeId, Error> get_entity_type(Context &ctx, TypedEntityRef entity_ref) noexcept {
  struct Visitor {
    std::expected<id::TypeId, Error> operator()(entity::ValueDeclaration const *val_decl) {
      return val_decl->type_signature;
    }
    std::expected<id::TypeId, Error>
    operator()(entity::ValueDefinition<ast::expr::Expr> const *val_def) {
      return get_type(ctx, std::move(*val_def->value))->type_id();
    }
    std::expected<id::TypeId, Error> operator()(entity::TypeFormDefinition const *type_form_def) {
      return type_form_def->type;
    }
    std::expected<id::TypeId, Error>
    operator()(entity::MergedValueDefinition<ast::expr::Expr> const *v) {
      auto type_id =
          get_type(ctx, std::move(*v->value)).transform([](auto const &x) { return x.type_id(); });
      if (not type_id) {
        todo();
      }
      ctx.solver.add_constraint(constraint::SubtypeOf{*type_id, v->type_signature});
      return v->type_signature;
    }
    std::expected<id::TypeId, Error> operator()(entity::Binding const *binding) {
      if (not binding->type) {
        return ctx.ts.make_variable();
      }
      return *binding->type;
    }

    Context &ctx;
  };

  auto type = std::visit(Visitor{ctx}, entity_ref);
  if (not type) {
    todo();
  }
  auto [_, did_insert] = ctx.env.type_of_entity.insert({entity_ref, *type});
  assert(did_insert);
  return *type;
}

} // namespace

export namespace analyser {

std::expected<void, Error> typecheck(storage::ResolvedAST const &ast) noexcept {
  constraint::Solver solver;
  Context ctx{*ast.ts, solver, ast.entities, ast.tags, {}};
  std::vector<std::pair<std::size_t, id::TypeId>> type_ids;
  for (std::size_t i = 0; i < ast.entities.size(); ++i) {
    auto &entity = ast.entities[i];
    auto result_opt = std::visit(
        [&ctx](auto const &entity) -> std::optional<std::expected<id::TypeId, Error>> {
          if constexpr (requires { get_entity_type(ctx, &entity); }) {
            return get_entity_type(ctx, &entity);
          } else {
            return std::nullopt;
          }
        },
        entity);
    if (not result_opt) {
      continue;
    }
    auto result = *result_opt;
    if (not result) {
      return std::unexpected(result.error());
    }
    type_ids.push_back({i, *result});
  }

  for (auto &[i, type_id] : type_ids) {
    std::cout << ctx.entity(id::EntityId{i}).name() << " : "
              << ctx.ts.type_name(ctx.entities, ctx.tags, type_id) << '\n';
  }

  std::cout << "CONSTRAINTS:\n";
  solver.solve(ctx.entities, ctx.tags, ctx.ts);

  return {};
}

} // namespace analyser

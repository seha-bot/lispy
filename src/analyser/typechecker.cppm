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

import constraint;
import entity;
import expr;
import formatter;
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

struct Env {
  void memoize(id::EntityId entity_id, id::TypeId type_id) {
    auto [_, did_insert] = type_of_entity.insert({entity_id, type_id});
    assert(did_insert);
  }

  std::unordered_map<id::EntityId, id::TypeId> type_of_entity;
};

struct Context {
  storage::TypeStorage &ts;
  constraint::Solver &solver;
  std::vector<entity::TypeFormDefinition> const &forms;
  std::vector<tag::Tag> const &tags;
  Env env;

  tag::Tag const &tag(id::TagId e) const { return tags[e.value]; }
};

std::expected<typed_expr::Expr, Error> get_type(Context &ctx, expr::Expr expr) noexcept {
  struct Visitor {
    std::expected<typed_expr::Expr, Error> operator()(expr::Application call) {
      auto function = get_type(ctx, std::move(*call.function));
      if (not function) {
        todo();
      }
      auto argument = get_type(ctx, std::move(*call.argument));
      if (not argument) {
        todo();
      }

      auto from_type_id = ctx.ts.make_variable();
      auto to_type_id = ctx.ts.make_variable();
      // TODO: This really doesn't need redundancy checking in store.
      auto invented_function_type_id = ctx.ts.store(type::Arrow{from_type_id, to_type_id});
      ctx.solver.add_constraint(constraint::SubtypeOf{
          invented_function_type_id,
          function->type_id(),
      });
      ctx.solver.add_constraint(constraint::SubtypeOf{
          function->type_id(),
          invented_function_type_id,
      });
      ctx.solver.add_constraint(constraint::SubtypeOf{
          argument->type_id(),
          from_type_id,
      });
      return typed_expr::Application{
          {to_type_id},
          std::make_unique<typed_expr::Expr>(*std::move(function)),
          std::make_unique<typed_expr::Expr>(typed_expr::Conversion{
              {from_type_id},
              std::make_unique<typed_expr::Expr>(*std::move(argument)),
          }),
      };
    }
    std::expected<typed_expr::Expr, Error> operator()(expr::Case) {
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
    std::expected<typed_expr::Expr, Error> operator()(expr::Variant variant) {
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
    std::expected<typed_expr::Expr, Error> operator()(expr::Pack pack) {
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
    std::expected<typed_expr::Expr, Error> operator()(expr::Lambda lambda) {
      auto body = get_type(ctx, std::move(*lambda.body));
      if (not body) {
        todo();
      }
      // TODO: You might not have to check for duplicates in store if binding_type_id represents a
      // fresh variable.
      auto const type_id = ctx.ts.store(type::Arrow{lambda.binding->type_id, body->type_id()});
      return typed_expr::Lambda{
          {type_id},
          std::move(lambda.captures),
          std::move(lambda.binding),
          std::make_unique<typed_expr::Expr>(*std::move(body)),
      };
    }
    std::expected<typed_expr::Expr, Error> operator()(expr::TVLambda tv_lambda) {
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
    std::expected<typed_expr::Expr, Error> operator()(expr::ValueReference value_ref) {
      auto const type_id = ctx.env.type_of_entity.at(value_ref.value_entity_id);
      return typed_expr::ValueReference{{type_id}, value_ref.value_entity_id};
    }
    std::expected<typed_expr::Expr, Error> operator()(expr::BindingReference binding_ref) {
      return typed_expr::BindingReference{
          {binding_ref.binding.get().type_id},
          binding_ref.binding,
      };
    }

    Context &ctx;
  };
  return std::visit(Visitor{ctx}, std::move(expr));
}

using ValueEntityBase = std::variant<         //
    entity::ValueDeclaration,                 //
    entity::ValueDefinition<expr::Expr>,      //
    entity::MergedValueDefinition<expr::Expr> //
    >;

struct ValueEntity : ValueEntityBase {
  using ValueEntityBase::ValueEntityBase;
};

std::expected<constraint::TypedValueEntity, Error> typecheck_entity(Context &ctx,
                                                                    ValueEntity entity) noexcept {
  struct Visitor {
    std::expected<constraint::TypedValueEntity, Error>
    operator()(entity::ValueDeclaration val_decl) {
      return val_decl;
    }
    std::expected<constraint::TypedValueEntity, Error>
    operator()(entity::ValueDefinition<expr::Expr> definition) {
      auto value = get_type(ctx, std::move(*definition.value));
      if (not value) {
        todo();
      }
      return entity::MergedValueDefinition<typed_expr::Expr>{
          .name = std::move(definition.name),
          .type_signature = value->type_id(),
          .value = std::make_unique<typed_expr::Expr>(*std::move(value)),
      };
    }
    std::expected<constraint::TypedValueEntity, Error>
    operator()(entity::MergedValueDefinition<expr::Expr> definition) {
      auto value = get_type(ctx, std::move(*definition.value));
      if (not value) {
        todo();
      }
      ctx.solver.add_constraint(constraint::SubtypeOf{value->type_id(), definition.type_signature});
      return entity::MergedValueDefinition<typed_expr::Expr>{
          .name = std::move(definition.name),
          .type_signature = definition.type_signature,
          .value = std::make_unique<typed_expr::Expr>(typed_expr::Conversion{
              {definition.type_signature},
              std::make_unique<typed_expr::Expr>(*std::move(value)),
          }),
      };
    }

    Context &ctx;
  };

  return std::visit(Visitor{ctx}, std::move(entity));
}

} // namespace

export namespace analyser {

std::expected<void, Error>
typecheck(storage::TypeStorage &ts, std::vector<tag::Tag> const &tags,
          std::vector<entity::TypeFormDefinition> const &forms,
          std::vector<entity::ModuleEntity<expr::Expr>> entities) noexcept {
  constraint::Solver solver;
  Context ctx{ts, solver, forms, tags, {}};
  std::vector<constraint::TypedValueEntity> typed_entities;
  for (std::size_t i = 0; i < entities.size(); ++i) {
    auto &entity = entities[i];
    auto result_opt = std::visit(
        [&](auto &entity) -> std::optional<std::expected<constraint::TypedValueEntity, Error>> {
          if constexpr (requires { typecheck_entity(ctx, std::move(entity)); }) {
            return typecheck_entity(ctx, std::move(entity))
                .transform([&](constraint::TypedValueEntity typed_entity) {
                  ctx.env.memoize(id::EntityId{{.value = i}}, typed_entity.type_id());
                  return typed_entity;
                });
          } else {
            return std::nullopt;
          }
        },
        entity);
    if (not result_opt) {
      continue;
    }
    auto result = *std::move(result_opt);
    if (not result) {
      return std::unexpected(result.error());
    }
    typed_entities.push_back(*std::move(result));
  }

  for (auto &entity : typed_entities) {
    std::cout << entity.name() << " : "
              << formatter::type_name({ctx.ts, forms, ctx.tags}, entity.type_id()) << '\n';
  }

  std::cout << "CONSTRAINTS:\n";
  solver.solve(std::move(typed_entities), forms, ctx.tags, ctx.ts);

  return {};
}

} // namespace analyser

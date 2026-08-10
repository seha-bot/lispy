module;

#include <algorithm>
#include <cassert>
#include <cstddef>
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
import tag;
import type;
import type_storage;
import typed_expr;

namespace {

using TypedValueEntityBase =
    std::variant<entity::ValueDeclaration, entity::MergedValueDefinition<typed_expr::Expr>>;

struct TypedValueEntity : TypedValueEntityBase {
  using TypedValueEntityBase::TypedValueEntityBase;

  id::TypeId type_id() const {
    return std::visit([](auto &x) { return x.type_signature; }, *this);
  }

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }
};

struct Env {
  void memoize(id::EntityId entity_id, id::TypeId type_id) {
    auto [_, did_insert] = type_of_entity.insert({entity_id, type_id});
    assert(did_insert);
  }

  std::unordered_map<id::EntityId, id::TypeId, std::hash<id::Id<id::Domain::entity>>>
      type_of_entity;
};

struct Context {
  storage::TypeStorage &ts;
  constraint::Solver &solver;
  Env &env;
};

std::vector<tag::Tag> const *tags;
std::vector<entity::TypeFormDefinition> const *forms;

typed_expr::Expr get_type(Context &ctx, expr::Expr expr) noexcept {
  struct Visitor {
    typed_expr::Expr operator()(expr::Application call) {
      auto function = get_type(ctx, std::move(*call.function));
      auto argument = get_type(ctx, std::move(*call.argument));

      // auto [from_id, to_id] = [&] {
      //   // // std::cout << "DEBUG " << formatter::type_name({ctx.ts, *forms, *tags},
      //   function.type_id()) << '\n';
      //   // if (auto *arr = std::get_if<type::Arrow>(&ctx.ts.read(function.type_id()))) {
      //   //   return std::make_pair(arr->from_id, arr->to_id);
      //   // }
      //   // // throw;
      //   id::TypeId const from_id = ctx.ts.make_variable();
      //   id::TypeId const to_id = ctx.ts.make_variable();
      //   // TODO: This really doesn't need redundancy checking in store.
      //   auto invented_function_type_id = ctx.ts.store(type::Arrow{from_id, to_id});
      //   ctx.solver.add_constraint(constraint::SubtypeOf{
      //       invented_function_type_id,
      //       function.type_id(),
      //   });
      //   ctx.solver.add_constraint(constraint::SubtypeOf{
      //       function.type_id(),
      //       invented_function_type_id,
      //   });
      //   return std::make_pair(from_id, to_id);
      // }();

      id::TypeId const from_id = ctx.ts.make_variable();
      id::TypeId const to_id = ctx.ts.make_variable();
      // TODO: This really doesn't need redundancy checking in store.
      auto invented_function_type_id = ctx.ts.store(type::Arrow{from_id, to_id});
      ctx.solver.add_constraint(constraint::SubtypeOf{
          function.type_id(),
          invented_function_type_id,
      });
      auto invented_function = typed_expr::Conversion{
          {invented_function_type_id},
          std::make_unique<typed_expr::Expr>(std::move(function)),
      };

      ctx.solver.add_constraint(constraint::SubtypeOf{
          argument.type_id(),
          from_id,
      });
      return typed_expr::Application{
          {to_id},
          std::make_unique<typed_expr::Expr>(std::move(invented_function)),
          std::make_unique<typed_expr::Expr>(typed_expr::Conversion{
              {from_id},
              std::make_unique<typed_expr::Expr>(std::move(argument)),
          }),
      };
    }
    typed_expr::Expr operator()(expr::Case case_) {
      auto scrutinee = get_type(ctx, std::move(*case_.scrutinee));

      std::vector<typed_expr::Choice> choices;
      auto const type_id = ctx.ts.make_variable();
      for (auto &[pattern, arm] : case_.choices) {
        auto typed_arm = get_type(ctx, std::move(arm));

        ctx.solver.add_constraint(constraint::SubtypeOf{
            typed_arm.type_id(),
            type_id,
        });

        choices.push_back({
            .pattern = std::move(pattern),
            .arm = std::move(typed_arm),
        });
      }

      return typed_expr::Case{
          {type_id},
          std::make_unique<typed_expr::Expr>(std::move(scrutinee)),
          std::move(choices),
      };
    }
    typed_expr::Expr operator()(expr::TaggedValue v) {
      auto value = get_type(ctx, std::move(*v.value));
      auto const type_id = ctx.ts.store(type::Union{std::vector{type::Element{
          .tag_id = v.tag_id,
          .type_id = value.type_id(),
      }}});
      return typed_expr::TaggedValue{
          {type_id},
          v.tag_id,
          std::make_unique<typed_expr::Expr>(std::move(value)),
      };
    }
    typed_expr::Expr operator()(expr::Pack pack) {
      std::vector<type::Element> elements;
      std::vector<typed_expr::TaggedValue> tagged_values;
      for (auto &[tag_id, value] : pack.tagged_values) {
        auto typed_value = get_type(ctx, std::move(*value));

        elements.push_back({
            .tag_id = tag_id,
            .type_id = typed_value.type_id(),
        });
        tagged_values.push_back({
            .tag_id = tag_id,
            .value = std::make_unique<typed_expr::Expr>(std::move(typed_value)),
        });
      }

      auto const type_id = ctx.ts.store(type::Struct{std::move(elements)});
      return typed_expr::Pack{{type_id}, std::move(tagged_values)};
    }
    typed_expr::Expr operator()(expr::Lambda lambda) {
      auto body = get_type(ctx, std::move(*lambda.body));
      // TODO: You might not have to check for duplicates in store if binding_type_id represents a
      // fresh variable.
      auto const type_id = ctx.ts.store(type::Arrow{lambda.binding->type_id, body.type_id()});
      return typed_expr::Lambda{
          {type_id},
          std::move(lambda.captures),
          std::move(lambda.binding),
          std::make_unique<typed_expr::Expr>(std::move(body)),
      };
    }
    typed_expr::Expr operator()(expr::TVLambda tv_lambda) {
      // TODO: Figure out kind inference.
      // auto binding_type = get_entity_kind(ctx, tv_lambda.binding_id);
      // if (not binding_type) {
      //   todo();
      // }

      auto body = get_type(ctx, std::move(*tv_lambda.body));
      auto const type_id = ctx.ts.store(type::ForAll{body.type_id()});
      return typed_expr::TVLambda{{type_id}, std::make_unique<typed_expr::Expr>(std::move(body))};
    }
    typed_expr::Expr operator()(expr::ValueReference value_ref) {
      auto const type_id = ctx.env.type_of_entity.at(value_ref.value_entity_id);
      return typed_expr::ValueReference{{type_id}, value_ref.value_entity_id};
    }
    typed_expr::Expr operator()(expr::BindingReference binding_ref) {
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

TypedValueEntity typecheck_entity(Context &ctx, ValueEntity entity) noexcept {
  struct Visitor {
    TypedValueEntity operator()(entity::ValueDeclaration val_decl) { return val_decl; }
    TypedValueEntity operator()(entity::ValueDefinition<expr::Expr> definition) {
      auto value = get_type(ctx, std::move(*definition.value));
      return entity::MergedValueDefinition<typed_expr::Expr>{
          .name = std::move(definition.name),
          .type_signature = value.type_id(),
          .value = std::make_unique<typed_expr::Expr>(std::move(value)),
      };
    }
    TypedValueEntity operator()(entity::MergedValueDefinition<expr::Expr> definition) {
      auto value = get_type(ctx, std::move(*definition.value));
      ctx.solver.add_constraint(constraint::SubtypeOf{value.type_id(), definition.type_signature});
      return entity::MergedValueDefinition<typed_expr::Expr>{
          .name = std::move(definition.name),
          .type_signature = definition.type_signature,
          .value = std::make_unique<typed_expr::Expr>(typed_expr::Conversion{
              {definition.type_signature},
              std::make_unique<typed_expr::Expr>(std::move(value)),
          }),
      };
    }

    Context &ctx;
  };

  return std::visit(Visitor{ctx}, std::move(entity));
}

} // namespace

namespace analyser {

export std::vector<TypedValueEntity>
typecheck(storage::TypeStorage &ts, std::vector<tag::Tag> const &tags,
          std::vector<entity::TypeFormDefinition> const &forms,
          std::vector<entity::ModuleEntity<expr::Expr>> entities) noexcept {
  ::forms = &forms;
  ::tags = &tags;
  Env env;
  std::vector<TypedValueEntity> typed_entities;
  for (std::size_t i = 0; i < entities.size(); ++i) {
    auto &entity = entities[i];

    constraint::Solver solver;

    auto typed_entity_opt = std::visit(
        [&](auto &entity) -> std::optional<TypedValueEntity> {
          Context ctx{ts, solver, env};
          if constexpr (requires { typecheck_entity(ctx, std::move(entity)); }) {
            auto var_id = ts.make_variable();
            env.memoize(id::EntityId{{.value = i}}, var_id);
            auto typed_entity = typecheck_entity(ctx, std::move(entity));
            solver.add_constraint(constraint::SubtypeOf{var_id, typed_entity.type_id()});
            solver.add_constraint(constraint::SubtypeOf{typed_entity.type_id(), var_id});
            return typed_entity;
          } else {
            return std::nullopt;
          }
        },
        entity);
    if (not typed_entity_opt) {
      continue;
    }
    auto typed_entity = *std::move(typed_entity_opt);

    std::cout << typed_entity.name() << " : "
              << formatter::type_name({ts, forms, tags}, typed_entity.type_id()) << '\n';
    std::cout << "CONSTRAINTS:\n";
    solver.solve(
        std::cout,
        [&](std::ostream &os, id::TypeId id) { os << formatter::type_name({ts, forms, tags}, id); },
        forms, ts);
    std::cout << "DONE.\n";
    std::cout << typed_entity.name() << " : "
              << formatter::type_name({ts, forms, tags}, typed_entity.type_id()) << '\n';

    typed_entities.push_back(std::move(typed_entity));
  }

  return typed_entities;
}

} // namespace analyser

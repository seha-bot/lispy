#include "typechecker.hpp"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <expected>
#include <functional>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "storage/resolved.hpp"
#include "storage/type_storage.hpp"
#include "todo.hpp"

namespace {

using TypedEntityRefBase = std::variant<        //
    ast::entity::ValueDeclaration const *,      //
    ast::entity::ValueDefinition const *,       //
    ast::entity::MergedValueDefinition const *, //
    ast::entity::Binding const *                //
    >;
struct TypedEntityRef : TypedEntityRefBase {
  using TypedEntityRefBase::variant;
};

} // namespace

template <> struct std::hash<TypedEntityRef> {
  std::size_t operator()(TypedEntityRef const &e) const noexcept {
    return std::visit([](auto p) { return std::hash<decltype(p)>{}(p); }, e);
  }
};

namespace analyser {

namespace {

// TODO: There is a class TypeEnv in storage which is very similar to this.
struct Env {
  std::unordered_map<ast::expr::Expr const *, ast::TypeId> type_of;
  std::unordered_map<TypedEntityRef, ast::TypeId> type_of_entity;
};

struct Context {
  storage::TypeStorage &ts;
  std::vector<ast::entity::ModuleEntity> const &entities;
  std::vector<ast::Tag> const &tags;
  Env env;

  ast::entity::ModuleEntity const &entity(ast::EntityId e) const { return entities[e.value]; }
  ast::Tag const &tag(ast::TagId e) const { return tags[e.value]; }

  auto fun() const {
    return [this](ast::EntityId id) -> ast::entity::ModuleEntity const & { return entity(id); };
  }

  std::string type_name(ast::TypeId t) const {
    if (auto a = std::get_if<ast::type::NamedTypeReference>(&ts.read(t).unnamed_part())) {
      return entity(a->definition_id).name();
    } else if (auto b = std::get_if<ast::type::Arrow>(&ts.read(t).unnamed_part())) {
      return "(" + type_name(b->from_id) + ") -> " + type_name(b->to_id);
    } else if (auto c = std::get_if<ast::type::ForAll>(&ts.read(t).unnamed_part())) {
      return "\\." + type_name(c->type_id);
    } else if (auto d = std::get_if<ast::type::DeBruijnIndex>(&ts.read(t).unnamed_part())) {
      return std::to_string(d->value);
    } else if (auto s = std::get_if<ast::type::Struct>(&ts.read(t).unnamed_part())) {
      if (s->elements.empty()) {
        return "{}";
      }
      std::string str = "{";
      for (auto &[tag_id, type_id] : s->elements) {
        str += " " + tag(tag_id).name.substr(1) + ": " + type_name(type_id) + ";";
      }
      return str + " }";
    } else if (auto v = std::get_if<ast::type::Variant>(&ts.read(t).unnamed_part())) {
      std::string str = "[";
      for (auto &[tag_id, type_id] : v->elements) {
        str += " " + tag(tag_id).name.substr(1) + ": " + type_name(type_id) + ";";
      }
      return str + " ]";
    }
    return "#" + ts.to_string(t);
  }
};

std::expected<ast::TypeId, Error> get_entity_type(Context &ctx, TypedEntityRef entity_ref) noexcept;

std::expected<ast::TypeId, Error> get_type(Context &ctx, ast::expr::Expr const &expr) noexcept {
  struct Visitor {
    std::expected<ast::TypeId, Error> operator()(ast::expr::Application const &call) {
      auto function_type_id = get_type(ctx, *call.function);
      if (not function_type_id) {
        todo();
      }
      auto argument_type_id = get_type(ctx, *call.argument);
      if (not argument_type_id) {
        todo();
      }

      auto type_id = ctx.ts.make_variable();
      ctx.ts.add_constraint(constraint::SubtypeOf{
          ctx.ts.store(ast::type::Arrow{*argument_type_id, type_id}),
          *function_type_id,
      });
      return type_id;
    }
    std::expected<ast::TypeId, Error> operator()(ast::expr::Case const &) {
      todo();
      // auto scrutinee_type = get_type(ctx, *case_.scrutinee);
      // if (not scrutinee_type) {
      //   todo();
      // }
      //
      // std::optional<ast::TypeId> common_arm_type;
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
    std::expected<ast::TypeId, Error> operator()(ast::expr::Variant const &variant) {
      if (variant.value) {
        auto value_type = get_type(ctx, **variant.value);
        if (not value_type) {
          todo();
        }
        return ctx.ts.store(ast::type::Variant{std::vector{ast::type::Element{
            .tag_id = variant.tag_id,
            .type_id = *value_type,
        }}});
      } else {
        return ctx.ts.store(ast::type::Variant{std::vector{ast::type::Element{
            .tag_id = variant.tag_id,
            .type_id = ast::TypeId::unit_id,
        }}});
      }
    }
    std::expected<ast::TypeId, Error> operator()(ast::expr::Pack const &pack) {
      std::vector<ast::type::Element> elements;
      for (auto &[tag_id, value] : pack.tagged_values) {
        auto type_id = get_type(ctx, *value);
        if (not type_id) {
          todo();
        }

        elements.push_back({
            .tag_id = tag_id,
            .type_id = *type_id,
        });
      }

      std::ranges::sort(elements, {}, [](auto &e) { return e.tag_id; });
      return ctx.ts.store(ast::type::Struct{std::move(elements)});
    }
    std::expected<ast::TypeId, Error> operator()(ast::expr::Lambda const &lambda) {
      auto binding_type_id = get_entity_type(ctx, lambda.binding.get());
      if (not binding_type_id) {
        todo();
      }
      auto body_type_id = get_type(ctx, *lambda.body);
      if (not body_type_id) {
        todo();
      }
      // TODO: You might not have to check for duplicates in store if binding_type_id represents a
      // fresh variable.
      return ctx.ts.store(ast::type::Arrow{*binding_type_id, *body_type_id});
    }
    std::expected<ast::TypeId, Error> operator()(ast::expr::TVLambda const &tv_lambda) {
      // TODO: Figure out kind inference.
      // auto binding_type = get_entity_kind(ctx, tv_lambda.binding_id);
      // if (not binding_type) {
      //   todo();
      // }

      auto body_type_id = get_type(ctx, *tv_lambda.body);
      if (not body_type_id) {
        todo();
      }
      return ctx.ts.store(ast::type::ForAll{*body_type_id});
    }
    std::expected<ast::TypeId, Error> operator()(ast::expr::ValueReference const &value_ref) {
      auto &entity = ctx.entity(value_ref.value_entity_id);
      auto *dec = std::get_if<ast::entity::ValueDeclaration>(&entity);
      auto *def = std::get_if<ast::entity::ValueDefinition>(&entity);
      auto *mer = std::get_if<ast::entity::MergedValueDefinition>(&entity);
      if (dec) {
        return ctx.env.type_of_entity[dec];
      } else if (def) {
        return ctx.env.type_of_entity[def];
      } else if (mer) {
        return ctx.env.type_of_entity[mer];
      } else {
        std::unreachable();
      }
    }
    std::expected<ast::TypeId, Error> operator()(ast::expr::BindingReference const &binding_ref) {
      return ctx.env.type_of_entity[&binding_ref.binding.get()];
    }

    Context &ctx;
  };
  return std::visit(Visitor{ctx}, expr);
}

std::expected<ast::TypeId, Error> get_entity_type(Context &ctx,
                                                  TypedEntityRef entity_ref) noexcept {
  struct Visitor {
    std::expected<ast::TypeId, Error> operator()(ast::entity::ValueDeclaration const *val_decl) {
      return val_decl->type_signature;
    }
    std::expected<ast::TypeId, Error> operator()(ast::entity::ValueDefinition const *val_def) {
      return get_type(ctx, *val_def->value);
    }
    std::expected<ast::TypeId, Error>
    operator()(ast::entity::TypeFormDefinition const *type_form_def) {
      return type_form_def->type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::entity::MergedValueDefinition const *v) {
      auto type_id = get_type(ctx, *v->value);
      if (not type_id) {
        todo();
      }
      ctx.ts.add_constraint(constraint::SubtypeOf{*type_id, v->type_signature});
      return v->type_signature;
    }
    std::expected<ast::TypeId, Error> operator()(ast::entity::Binding const *binding) {
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

// TODO: Rename.
struct Visitor {
  std::optional<std::expected<ast::TypeId, Error>> operator()(auto const &entity) {
    if constexpr (requires { get_entity_type(ctx, &entity); }) {
      return get_entity_type(ctx, &entity);
    } else {
      return std::nullopt;
    }
  }

  Context &ctx;
};

std::expected<storage::TypeEnv, Error> typecheck(storage::ResolvedAST const &ast) noexcept {
  Context ctx{*ast.ts, ast.entities, ast.tags, {}};
  std::vector<ast::TypeId> type_ids;
  for (auto &entity : ast.entities) {
    auto result_opt = std::visit(Visitor{ctx}, entity);
    if (not result_opt) {
      continue;
    }
    auto result = *result_opt;
    if (not result) {
      return std::unexpected(result.error());
    }
    type_ids.push_back(*result);
  }

  for (std::size_t i = 0; i < ast.entities.size(); ++i) {
    auto &type_id = type_ids[i];
    std::cout << ctx.entity(ast::EntityId{i}).name() << " : " << ctx.type_name(type_id) << '\n';
  }

  // TODO: This should not be here, but at the end of each function.
  // if (ctx.ts.has_uninferred_types()) {
  //   todo();
  // }

  return storage::TypeEnv{ctx.env.type_of};
}

} // namespace analyser

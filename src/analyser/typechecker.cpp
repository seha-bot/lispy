#include "typechecker.hpp"

#include <cassert>
#include <cstddef>
#include <expected>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "storage/resolved.hpp"
#include "todo.hpp"

namespace analyser {

namespace {

// FIX: Use TypeEnv from storage instead of this.
struct Env {
  std::unordered_map<ast::Expr const *, ast::TypeId> type_of;
  // TODO: this is really not needed though.
  std::unordered_map<ast::EntityId, ast::TypeId> type_of_entity;
};

struct Context {
  storage::TypeStorage &ts;
  std::vector<ast::Entity> const &entities;
  std::vector<ast::Tag> const &tags;
  Env env;

  ast::Entity const &entity(ast::EntityId e) const { return entities[e.value]; }
  ast::Tag const &tag(ast::TagId e) const { return tags[e.id]; }

  auto fun() const {
    return [this](ast::EntityId id) -> ast::Entity const & { return entity(id); };
  }

  std::string type_name(ast::TypeId t) const {
    if (auto a = std::get_if<ast::type::NamedReference>(&ts.read(t).unnamed_part())) {
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

std::expected<ast::TypeId, Error> get_entity_type(Context &ctx, ast::EntityId entity_id) noexcept;

std::expected<ast::TypeId, Error> get_type(Context &ctx, ast::Expr const &expr) noexcept {
  struct Visitor {
    std::expected<ast::TypeId, Error> operator()(ast::Call const &call) {
      auto callee_type = get_type(ctx, *call.callee);
      if (not callee_type) {
        todo();
      }

      // TODO: this may be more efficient.
      // auto return_type = *callee_type;
      // for (auto& arg : call.arguments) {
      //     auto return_type_str = ctx.tip(return_type);

      //     auto read_return_type = [&] -> ast::type::Type const& {
      //         return ctx.ts.read(return_type);
      //     };
      //     if (std::holds_alternative<ast::type::TypeArrow>(read_return_type())) {
      //         auto arr = [&] -> ast::type::TypeArrow const& {
      //             return std::get<ast::type::TypeArrow>(read_return_type());
      //         };

      //         auto arg_type = get_type(ctx, arg);
      //         if (not arg_type) {
      //             todo();
      //         }

      //         bool const did_merge = ctx.ts.merge(*arg_type, arr().from);
      //         if (not did_merge) {
      //             todo();
      //         }

      //         return_type = arr().to;
      //     } else {
      //         todo();
      //     }
      // }

      auto const return_type = ctx.ts.make_variable();
      auto callee_type_calculated = return_type;
      for (auto &arg : call.arguments | std::views::reverse) {
        auto arg_type = get_type(ctx, arg);
        if (not arg_type) {
          todo();
        }

        callee_type_calculated = ctx.ts.store(ast::type::Arrow{*arg_type, callee_type_calculated});
      }

      auto const merged_type_id = ctx.ts.merge(ctx.fun(), *callee_type, callee_type_calculated);
      if (not merged_type_id) {
        todo();
      }
      return return_type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::Case const &case_) {
      auto scrutinee_type = get_type(ctx, *case_.scrutinee);
      if (not scrutinee_type) {
        todo();
      }

      std::optional<ast::TypeId> common_arm_type;
      for (auto &[pattern, arm] : case_.choices) {
        // FIX: pattern binding should be checked against the declared type.
        pattern;

        auto arm_type = get_type(ctx, arm);
        if (not arm_type) {
          todo();
        }

        if (common_arm_type) {
          auto const merged_type_id = ctx.ts.merge(ctx.fun(), *common_arm_type, *arm_type);
          if (not merged_type_id) {
            todo();
          }
          common_arm_type = *merged_type_id;
        } else {
          common_arm_type = *arm_type;
        }
      }

      if (not common_arm_type) {
        todo();
      }
      return *common_arm_type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::Variant const &variant) {
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
    std::expected<ast::TypeId, Error> operator()(ast::Pack const &pack) {
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
    std::expected<ast::TypeId, Error> operator()(ast::Lambda const &lambda) {
      auto binding_type = get_entity_type(ctx, lambda.parameter);
      if (not binding_type) {
        todo();
      }

      auto body_type = get_type(ctx, *lambda.body);
      if (not body_type) {
        todo();
      }

      // TODO: in this situation you don't have to check for duplicates with ts.store().
      // Make an unchecked version.
      return ctx.ts.store(ast::type::Arrow{*binding_type, *body_type});
    }
    std::expected<ast::TypeId, Error> operator()(ast::TVLambda const &tv_lambda) {
      // TODO: Figure out kind inference.
      // auto binding_type = get_entity_kind(ctx, tv_lambda.binding_id);
      // if (not binding_type) {
      //   todo();
      // }

      auto body_type = get_type(ctx, *tv_lambda.body);
      if (not body_type) {
        todo();
      }

      // TODO: in this situation you don't have to check for duplicates with ts.store().
      // Make an unchecked version.
      return ctx.ts.store(ast::type::ForAll{*body_type});
    }
    std::expected<ast::TypeId, Error> operator()(ast::EntityReference const &entity_ref) {
      return get_entity_type(ctx, entity_ref.id);
    }

    Context &ctx;
  };

  auto [iter, did_insert] = ctx.env.type_of.insert({&expr, ctx.ts.make_variable()});
  assert(did_insert);

  auto type = std::visit(Visitor{ctx}, expr);
  if (not type) {
    todo();
  }

  auto const merged_type_id = ctx.ts.merge(ctx.fun(), iter->second, *type);
  if (not merged_type_id) {
    todo();
  }
  return *merged_type_id;
}

std::expected<ast::TypeId, Error> get_entity_type(Context &ctx, ast::EntityId entity_id) noexcept {
  struct Visitor {
    std::expected<ast::TypeId, Error> operator()(ast::ValueDefinition const &val_def) {
      return get_type(ctx, val_def.value);
    }
    std::expected<ast::TypeId, Error> operator()(ast::TypeFormDefinition const &type_form_def) {
      return type_form_def.type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::ValueDeclaration const &val_decl) {
      return val_decl.type_signature;
    }
    std::expected<ast::TypeId, Error> operator()(ast::MergedValueDefinition const &v) {
      auto type = get_type(ctx, v.value);
      if (not type) {
        todo();
      }

      auto const merged_type_id = ctx.ts.merge(ctx.fun(), v.type_signature, *type);
      if (not merged_type_id) {
        todo();
      }
      return *merged_type_id;
    }
    std::expected<ast::TypeId, Error> operator()(ast::ModuleDefinition const &) { todo(); }
    std::expected<ast::TypeId, Error> operator()(ast::Binding const &binding) {
      if (not binding.type) {
        return ctx.ts.make_variable();
      }
      return *binding.type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::TypeBinding const &) {
      // FIX: This is a hack.
      return ast::TypeId{};
    }

    Context &ctx;
  };

  auto [iter, did_insert] = ctx.env.type_of_entity.insert({entity_id, ctx.ts.make_variable()});
  auto entity_type = iter->second;
  if (not did_insert) {
    return entity_type;
  }

  auto type = std::visit(Visitor{ctx}, ctx.entities[entity_id.value]);
  if (not type) {
    todo();
  }

  auto const merged_type_id = ctx.ts.merge(ctx.fun(), entity_type, *type);
  if (not merged_type_id) {
    todo();
  }
  return *merged_type_id;
}

} // namespace

std::expected<storage::TypeEnv, Error> typecheck(storage::ResolvedAST const &ast) noexcept {
  Context ctx{*ast.ts, ast.entities, ast.tags, {}};
  std::vector<ast::TypeId> type_ids;
  for (std::size_t i = 0; i < ast.entities.size(); ++i) {
    auto res = get_entity_type(ctx, ast::EntityId{i});
    if (not res) {
      return std::unexpected(res.error());
    }
    type_ids.push_back(*res);
  }

  for (std::size_t i = 0; i < ast.entities.size(); ++i) {
    auto &type_id = type_ids[i];
    std::cout << ctx.entity(ast::EntityId{i}).name() << " : " << ctx.type_name(type_id) << '\n';
  }

  // TODO:
  // if (ctx.ts.has_uninferred_types()) {
  //   todo();
  // }

  return storage::TypeEnv{ctx.env.type_of};
}

} // namespace analyser

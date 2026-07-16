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
  Env env;

  ast::Entity const &entity(ast::EntityId e) const { return entities[e.value]; }

  std::string type_name(ast::TypeId t) const {
    if (auto a = std::get_if<ast::NamedType>(&ts.read(t))) {
      return entity(a->definition).name();
    } else if (auto b = std::get_if<ast::TypeArrow>(&ts.read(t))) {
      return "(" + type_name(b->from) + ") -> (" + type_name(b->to) + ")";
    }
    return ts.to_string(t);
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

      //     auto read_return_type = [&] -> ast::Type const& {
      //         return ctx.ts.read(return_type);
      //     };
      //     if (std::holds_alternative<ast::TypeArrow>(read_return_type())) {
      //         auto arr = [&] -> ast::TypeArrow const& {
      //             return std::get<ast::TypeArrow>(read_return_type());
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

        callee_type_calculated = ctx.ts.store(ast::TypeArrow{*arg_type, callee_type_calculated});
      }

      bool const did_merge = ctx.ts.merge(*callee_type, callee_type_calculated);
      if (not did_merge) {
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
          bool const did_merge = ctx.ts.merge(*common_arm_type, *arm_type);
          if (not did_merge) {
            todo();
          }
        } else {
          common_arm_type = *arm_type;
        }
      }

      if (not common_arm_type) {
        todo();
      }
      return *common_arm_type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::Constructor const &lcall) {
      // TODO: this will not evaluate the argument.
      return lcall.type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::Lambda const &lambda) {
      auto parameter_type = get_entity_type(ctx, lambda.parameter);
      if (not parameter_type) {
        todo();
      }

      auto body_type = get_type(ctx, *lambda.body);
      if (not body_type) {
        todo();
      }

      // TODO: in this situation you don't have to check for duplicates with ts.store().
      // Make an unchecked version.
      return ctx.ts.store(ast::TypeArrow{*parameter_type, *body_type});
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

  bool const did_merge = ctx.ts.merge(iter->second, *type);
  if (not did_merge) {
    todo();
  }

  return *type;
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

      bool const did_merge = ctx.ts.merge(v.type_signature, *type);
      if (not did_merge) {
        todo();
      }

      return *type;
    }
    std::expected<ast::TypeId, Error> operator()(ast::ModuleDefinition const &) { todo(); }
    std::expected<ast::TypeId, Error> operator()(ast::Binding const &binding) {
      if (not binding.type) {
        return ctx.ts.make_variable();
      }
      return *binding.type;
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

  bool const did_merge = ctx.ts.merge(entity_type, *type);
  if (not did_merge) {
    todo();
  }

  return *type;
}

} // namespace

std::expected<storage::TypeEnv, Error> typecheck(storage::ResolvedAST const &ast) noexcept {
  Context ctx{*ast.ts, ast.entities, {}};
  for (std::size_t i = 0; i < ast.entities.size(); ++i) {
    auto res = get_entity_type(ctx, ast::EntityId{i});
    if (not res) {
      return std::unexpected(res.error());
    }
    std::cout << ctx.entity(ast::EntityId{i}).name() << " : " << ctx.type_name(*res) << '\n';
  }

  return storage::TypeEnv{ctx.env.type_of};
}

} // namespace analyser

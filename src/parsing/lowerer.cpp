#include "lowerer.hpp"

#include "ast.hpp"
#include "context.hpp"
#include "pars.hpp"
#include "raw.hpp"
#include "storage/resolved.hpp"
#include "todo.hpp"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace compiler {

namespace {

// vacant is a cool word

std::expected<ast::Entity, Error> compile_shallow_entity(Context ctx,
                                                         ast::ShallowEntity entity) noexcept;

namespace shallow {

std::expected<ast::ValueDefinition, Error>
compile_shallow_entity(Context ctx, ast::ShallowValueDefinition value) {
  auto expr = parse(raw::expr_parser(std::move(ctx)), std::move(value.raw_value));
  if (not expr) {
    todo();
  }
  return ast::ValueDefinition{std::move(value.name), *std::move(expr)};
}

std::expected<ast::TypeFormDefinition, Error>
compile_shallow_entity(Context ctx, ast::ShallowTypeFormDefinition shallow_type_form) {
  auto type = parse(raw::type_parser(std::move(ctx)), std::move(shallow_type_form.raw_type));
  if (not type) {
    todo();
  }
  return ast::TypeFormDefinition{std::move(shallow_type_form.name), *std::move(type)};
}

std::expected<ast::ValueDeclaration, Error>
compile_shallow_entity(Context ctx, ast::ShallowValueDeclaration shallow_value_declaration) {
  auto type_signature = parse(raw::type_parser(std::move(ctx)),
                              std::move(shallow_value_declaration.raw_type_signature));
  if (not type_signature) {
    todo();
  }
  return ast::ValueDeclaration{std::move(shallow_value_declaration.name),
                               *std::move(type_signature)};
}

std::expected<ast::MergedValueDefinition, Error>
compile_shallow_entity(Context ctx,
                       ast::ShallowMergedValueDefinition shallow_merged_value_definition) {
  auto type_signature =
      parse(raw::type_parser(ctx), std::move(shallow_merged_value_definition.raw_type_signature));
  if (not type_signature) {
    todo();
  }

  auto expr =
      parse(raw::expr_parser(std::move(ctx)), std::move(shallow_merged_value_definition.raw_value));
  if (not expr) {
    todo();
  }

  return ast::MergedValueDefinition{std::move(shallow_merged_value_definition.name),
                                    *std::move(type_signature), *std::move(expr)};
}

std::expected<ast::ModuleDefinition, Error>
compile_shallow_entity(Context ctx, ast::ShallowModuleDefinition module) {
  std::vector<ast::ShallowEntity> shallow_entities;
  std::unordered_map<std::string, ast::EntityId> scope_entities;
  std::vector<ast::EntityId> module_entities;

  for (auto &raw_entity : module.raw_entities.elements()) {
    auto shallow_entity = parse(raw::module_entity_parser(), std::move(raw_entity));
    if (not shallow_entity) {
      todo();
    }

    auto name = std::visit([](auto &entity) { return static_cast<std::string>(entity.name); },
                           *shallow_entity);

    if (scope_entities.contains(name)) {
      auto id = scope_entities.at(name);
      auto i = static_cast<std::size_t>(std::ranges::find(module_entities, id) -
                                        module_entities.begin());
      auto *decl = std::get_if<ast::ShallowValueDeclaration>(&shallow_entities.at(i));
      auto *def = std::get_if<ast::ShallowValueDefinition>(&*shallow_entity);
      if (decl and def) {
        shallow_entities.at(i) = ast::ShallowMergedValueDefinition{
            std::move(decl->name), std::move(decl->raw_type_signature), std::move(def->raw_value)};
      } else {
        todo();
      }
    } else {
      shallow_entities.push_back(std::move(*shallow_entity));
      auto id = ctx.es.reserve();
      scope_entities.insert({std::move(name), id});
      module_entities.push_back(id);
    }
  }

  auto module_ctx = ctx.with_names(std::move(scope_entities));

  // TODO: reserve
  for (std::size_t i = 0; i < shallow_entities.size(); ++i) {
    auto res = compiler::compile_shallow_entity(module_ctx, std::move(shallow_entities[i]));
    if (not res) {
      return std::unexpected(res.error());
    }
    ctx.es.store(module_entities[i], *std::move(res));
  }

  return ast::ModuleDefinition{std::move(module.name), std::move(module_entities)};
}

} // namespace shallow

// TODO: don't name this the same way as the overloads in shallow::
std::expected<ast::Entity, Error> compile_shallow_entity(Context ctx,
                                                         ast::ShallowEntity entity) noexcept {
  return std::visit(
      [ctx](auto unwrapped_entity) -> std::expected<ast::Entity, Error> {
        auto res = shallow::compile_shallow_entity(ctx, std::move(unwrapped_entity));
        if (not res) {
          return std::unexpected(res.error());
        }
        return *std::move(res);
      },
      std::move(entity));
}

} // namespace

std::expected<storage::ResolvedAST, Error> lower_ast(std::string filename,
                                                     std::vector<ast::RawExpr> ast) noexcept {
  auto ts = std::make_unique<storage::TypeStorage>();
  EntityStorage storage;
  auto shallow_module =
      ast::ShallowModuleDefinition{std::move(filename), ast::List(std::move(ast), ast::Source{})};
  auto module = shallow::compile_shallow_entity(Context(*ts, storage), std::move(shallow_module));
  if (not module) {
    todo();
  }

  return storage::ResolvedAST{*std::move(module), std::move(ts), storage.produce()};
}

} // namespace compiler

bool ast::TypeId::operator==(TypeId const &that) const { return m_rep->equal(*this, that); }
std::string ast::TypeId::to_string() const {
  return std::to_string(m_rep->representative(*this).m_id);
}

#include "lowerer.hpp"

#include <algorithm>
#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "context.hpp"
#include "pars.hpp"
#include "raw.hpp"
#include "shallow_ast.hpp"
#include "storage/resolved.hpp"
#include "todo.hpp"

namespace parser {

namespace {

// vacant is a cool word

namespace shallow {

std::expected<ast::ValueDefinition, Error> lower_entity(Context const &ctx,
                                                        shallow_ast::ShallowValueDefinition value) {
  auto expr = parse(raw::expr_parser(ctx), std::move(value.raw_value));
  if (not expr) {
    todo();
  }
  return ast::ValueDefinition{std::move(value.name), *std::move(expr)};
}

std::expected<ast::TypeFormDefinition, Error>
lower_entity(Context ctx, shallow_ast::ShallowTypeFormDefinition shallow_type_form) {
  auto type = parse(raw::type_parser(std::move(ctx)), std::move(shallow_type_form.raw_type));
  if (not type) {
    todo();
  }
  return ast::TypeFormDefinition{std::move(shallow_type_form.name), *std::move(type)};
}

std::expected<ast::ValueDeclaration, Error>
lower_entity(Context ctx, shallow_ast::ShallowValueDeclaration shallow_value_declaration) {
  auto type_signature = parse(raw::type_parser(std::move(ctx)),
                              std::move(shallow_value_declaration.raw_type_signature));
  if (not type_signature) {
    todo();
  }
  return ast::ValueDeclaration{std::move(shallow_value_declaration.name),
                               *std::move(type_signature)};
}

std::expected<ast::MergedValueDefinition, Error>
lower_entity(Context const &ctx,
             shallow_ast::ShallowMergedValueDefinition shallow_merged_value_definition) {
  auto type_signature =
      parse(raw::type_parser(ctx), std::move(shallow_merged_value_definition.raw_type_signature));
  if (not type_signature) {
    todo();
  }

  auto expr = parse(raw::expr_parser(ctx), std::move(shallow_merged_value_definition.raw_value));
  if (not expr) {
    todo();
  }

  return ast::MergedValueDefinition{std::move(shallow_merged_value_definition.name),
                                    *std::move(type_signature), *std::move(expr)};
}

std::expected<ast::ModuleDefinition, Error>
lower_entity(Context ctx, shallow_ast::ShallowModuleDefinition shallow_module) {
  auto shallow_entities_result =
      parse(raw::shallow_entities_parser(), std::move(shallow_module.raw_entities));
  if (not shallow_entities_result) {
    todo();
  }

  auto &shallow_entities = *shallow_entities_result;
  std::unordered_map<std::string, std::pair<std::size_t, ast::EntityId>> entity_ids;

  for (std::size_t i = 0; i < shallow_entities.size(); ++i) {
    auto &shallow_entity = shallow_entities[i];
    auto &name = shallow_entity.name();
    if (auto it = entity_ids.find(name); it != entity_ids.end()) {
      auto [j, _] = it->second;
      auto *declaration = std::get_if<shallow_ast::ShallowValueDeclaration>(&shallow_entities[j]);
      auto *definition = std::get_if<shallow_ast::ShallowValueDefinition>(&shallow_entity);
      if (declaration and definition) {
        shallow_entities[j] = shallow_ast::ShallowMergedValueDefinition{
            std::move(declaration->name),
            declaration->raw_type_signature,
            definition->raw_value,
        };
      } else {
        todo();
      }
    } else {
      entity_ids.insert({name, {i, ctx.es.reserve()}});
    }
  }

  for (auto &[_, index_and_entity_id] : entity_ids) {
    auto [i, id] = index_and_entity_id;
    auto visitor = [&](auto shallow_entity) {
      // TODO: Figure out a way to remove the copy here.
      std::unordered_map<std::string, ast::EntityId> haha;
      for (auto &[k, v] : entity_ids) {
        haha[k] = v.second;
      }

      auto result =
          shallow::lower_entity(ctx.with_names(std::move(haha)), std::move(shallow_entity));
      if (not result) {
        todo();
      }
      ctx.es.store(id, *std::move(result));
    };
    std::visit(visitor, std::move(shallow_entities[i]));
  }

  std::vector<ast::EntityId> result;
  result.reserve(entity_ids.size());
  for (auto &[_, index_and_entity_id] : entity_ids) {
    result.push_back(index_and_entity_id.second);
  }
  return ast::ModuleDefinition{std::move(shallow_module.name), result};
}

} // namespace shallow

} // namespace

std::expected<storage::ResolvedAST, Error> lower_ast(std::string filename,
                                                     std::vector<ast::RawExpr> ast) noexcept {
  auto ts = std::make_unique<storage::TypeStorage>();
  storage::EntityStorage storage;
  auto module_definition = shallow::lower_entity(
      Context(*ts, storage),
      shallow_ast::ShallowModuleDefinition{filename, ast::List(std::move(ast), ast::Source{})});
  if (not module_definition) {
    todo();
  }
  return storage::ResolvedAST{*std::move(module_definition), std::move(ts), storage.produce()};
}

} // namespace parser

// FIX: this doesn't belong here!!!
bool ast::TypeId::operator==(TypeId const &that) const { return m_rep->equal(*this, that); }
std::string ast::TypeId::to_string() const {
  return std::to_string(m_rep->representative(*this).m_id);
}

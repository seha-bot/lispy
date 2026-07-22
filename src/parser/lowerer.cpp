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
#include "parsy.hpp"
#include "raw_ast.hpp"
#include "shallow_ast.hpp"
#include "storage/resolved.hpp"
#include "todo.hpp"

namespace parser {

// vacant is a cool word
namespace {

template <typename T>
auto const to = []<typename... Ts>(Ts &&...args) { return T(std::forward<Ts>(args)...); };

namespace raw {

template <typename A> using Parser = parsy::Parsy<ExprView, A>;

auto pure(auto value) { return parsy::pure<ExprView>(std::move(value)); }
auto pure_once(auto value) { return parsy::pure_once<ExprView>(std::move(value)); }

template <std::invocable<raw_ast::Atom const &> Pred>
Parser<ExprView> atom_where(parsy::MeaningfulPredicate<Pred> pred) {
  return parsy::satisfies<ExprView>(parsy::MeaningfulPredicate{
      .meaning = pred.meaning, .fn = [fn = std::move(pred.fn)](raw_ast::Expr const &expr) {
        auto *atom = std::get_if<raw_ast::Atom>(&expr);
        return atom and fn(*atom);
      }});
}

Parser<ExprView> atom(std::string meaning) {
  return atom_where(parsy::MeaningfulPredicate{
      .meaning = std::move(meaning),
      .fn = [](auto &) { return true; },
  });
}

Parser<ExprView> atom_exact(std::string name) {
  auto name_copy = name;
  return atom_where(parsy::MeaningfulPredicate{
      .meaning = '"' + std::move(name_copy) + '"',
      .fn = [name = std::move(name)](auto &atom) { return atom.name == name; },
  });
}

Parser<ExprView> atom_starting_with(char prefix) {
  return atom_where(parsy::MeaningfulPredicate{
      .meaning = "an atom starting with '" + std::string(1, prefix) + '\'',
      .fn = [prefix](auto &atom) { return atom.name.at(0) == prefix; },
  });
}

template <typename A> Parser<A> list(Parser<A> parser) {
  auto satisfy_list = satisfies<ExprView>(parsy::MeaningfulPredicate{
      .meaning = "a list",
      .fn = [](raw_ast::Expr const &expr) { return std::holds_alternative<raw_ast::List>(expr); },
  });

  auto parse_list_body = [parser = std::move(parser)](ExprView list_view) -> Parser<A> {
    return {[&parser, list_view](ExprView tokens) -> parsy::ParseResult<ExprView, A> {
      auto &list = *std::get_if<raw_ast::List>(&ExprView(list_view).head());
      auto list_content = ExprView{
          std::span<raw_ast::Expr>(list.elements.begin(), list.elements.end()),
          list_view.pos().first,
      };

      auto result = parse(parser, list_content);
      if (not result) {
        return std::move(result.error());
      }
      return {std::move(result.value()), tokens};
    }};
  };

  return std::move(satisfy_list) >> std::move(parse_list_body);
}

Parser<ast::TagId> tag_parser(Context ctx) {
  return atom_starting_with(':') | [ctx = std::move(ctx)](ExprView atom_view) -> ast::TagId {
    return ctx.es.get_tag(std::move(atom_view.head_as_atom().name));
  };
}

Parser<ast::TypeId> type_parser(Context ctx) noexcept {
  auto rec_type_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return type_parser(ctx); }); };

  auto name_defined = [ctx](raw_ast::Atom const &atom) { return !!ctx.lookup(atom.name); };
  auto defined_name = [name_defined] {
    return atom_where(parsy::MeaningfulPredicate{
        .meaning = "a defined type name",
        .fn = name_defined,
    });
  };

  auto lookup = [ctx](ExprView atom_view) -> ast::type::Unnamed {
    auto entity_id_opt = ctx.lookup(atom_view.head_as_atom().name);
    if (not entity_id_opt) {
      todo();
    }
    auto entity_id = *entity_id_opt;

    if (ctx.es.holds_alternative<ast::TypeBinding>(entity_id)) {
      return ast::type::DeBruijnIndex{ctx.type_binding_index(entity_id)};
    } else if (ctx.es.holds_alternative<ast::TypeFormDefinition>(entity_id)) {
      return ast::type::NamedReference{entity_id};
    } else {
      todo();
    }
  };

  auto arrow_parser = seq(to<ast::type::Arrow>, rec_type_parser(), rec_type_parser());

  auto forall_parser = [ctx] -> Parser<ast::type::ForAll> {
    // TODO: Factor this out because it's also used in expr_parser in 2 places.
    auto to_binding = [ctx](ExprView name_view) {
      auto &atom = name_view.head_as_atom();
      auto binding_id = ctx.es.reserve_store(ast::TypeBinding{atom.name});
      return std::make_pair(ctx.with_names({{std::move(atom.name), binding_id}}), binding_id);
    };

    auto binding_parser = atom("a type binding name") | to_binding;

    return std::move(binding_parser) >> [](std::pair<Context, ast::EntityId> p) {
      auto [new_ctx, binding_id] = p;
      new_ctx.push_type_binding(binding_id);
      return type_parser(new_ctx) | to<ast::type::ForAll>;
    };
  }();

  auto variant_parser = [ctx, rec_type_parser] -> Parser<ast::type::Variant> {
    auto element_parser = any(std::array{
        seq(to<ast::type::Element>, tag_parser(ctx), pure(ast::TypeId::unit_id)),
        list(seq(to<ast::type::Element>, tag_parser(ctx), rec_type_parser())),
    });

    return many(std::move(element_parser)) | [](std::vector<ast::type::Element> elements) {
      std::ranges::sort(elements, {}, [](auto &e) { return e.tag_id; });
      return elements;
    } | to<ast::type::Variant>;
  }();

  // FIX: Sort elements by tag_id. There is common functionality in variant_parser,
  // so factor it out.
  auto struct_parser = many(list(seq(to<ast::type::Element>, tag_parser(ctx), rec_type_parser()))) |
                       to<ast::type::Struct>;

  auto store = [ctx](ast::type::Type type) { return ctx.ts.store(std::move(type)); };

  return any(std::array{
      defined_name() | lookup | store,
      list(any(std::array{
          atom_exact("to") > cut(std::move(arrow_parser)) | store,
          atom_exact("forall") > cut(std::move(forall_parser)) | store,
          atom_exact("variant") > cut(std::move(variant_parser)) | store,
          atom_exact("struct") > cut(std::move(struct_parser)) | store,
          // atom_exact("tt-lambda") > std::move(tt_parser),
      })),
  });
}

Parser<ast::Expr> expr_parser(Context ctx) noexcept;

Parser<ast::Case::Choice> case_choice_parser(Context ctx) noexcept {
  auto pattern_bindings = [] {
    return many(atom("a pattern binding") < peek(atom("a pattern binding")));
  };
  auto arm = [](Context new_ctx) { return expr_parser(new_ctx); };

  return list(tag_parser(ctx) >> [=](ast::TagId tag_id) {
    return pattern_bindings() >> [=](std::vector<ExprView> binding_name_views) {
      std::vector<ast::EntityId> binding_ids;
      std::unordered_map<std::string, ast::EntityId> binding_names_to_ids;
      binding_ids.reserve(binding_name_views.size());
      binding_names_to_ids.reserve(binding_name_views.size());

      for (auto &binding_name_view : binding_name_views) {
        auto binding_name = std::move(binding_name_view.head_as_atom().name);
        auto binding_id = ctx.es.reserve_store(ast::Binding{binding_name, std::nullopt});
        binding_ids.push_back(binding_id);
        binding_names_to_ids.insert({std::move(binding_name), binding_id});
      }
      auto new_ctx = ctx.with_names(std::move(binding_names_to_ids));

      return seq(to<ast::Case::Choice>,
                 pure_once(ast::Case::Pattern(tag_id, std::move(binding_ids))), arm(new_ctx));
    };
  });
}

Parser<ast::Expr> special_parser(Context ctx) noexcept {
  auto rec_expr_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return expr_parser(ctx); }); };

  auto lambda_parser = [ctx] -> Parser<ast::Lambda> {
    auto to_binding = [ctx](ExprView name_view, std::optional<ast::TypeId> type) {
      auto &atom = name_view.head_as_atom();

      auto binding_id = ctx.es.reserve_store(ast::Binding{atom.name, type});
      return std::make_pair(ctx.with_names({{std::move(atom.name), binding_id}}), binding_id);
    };

    auto binding_parser = any(std::array{
        atom("a binding name") |
            [=](ExprView name_view) { return to_binding(name_view, std::nullopt); },
        list(cut(seq(to_binding, atom("a binding name"), type_parser(ctx)))),
    });

    return std::move(binding_parser) >> [](std::pair<Context, ast::EntityId> p) {
      auto [new_ctx, binding_id] = p;
      return expr_parser(new_ctx) | [new_ctx, binding_id](ast::Expr body) {
        std::vector<ast::EntityId> captures(new_ctx.captures().begin(), new_ctx.captures().end());
        return ast::Lambda{std::move(captures), binding_id,
                           std::make_unique<ast::Expr>(std::move(body))};
      };
    };
  }();

  auto tv_lambda_parser = [ctx] -> Parser<ast::TVLambda> {
    auto to_binding = [ctx](ExprView name_view) {
      auto &atom = name_view.head_as_atom();

      auto binding_id = ctx.es.reserve_store(ast::TypeBinding{atom.name});
      return std::make_pair(ctx.with_names({{std::move(atom.name), binding_id}}), binding_id);
    };

    auto binding_parser = atom("a type binding name") | to_binding;

    return std::move(binding_parser) >> [](std::pair<Context, ast::EntityId> p) {
      auto [new_ctx, binding_id] = p;
      new_ctx.push_type_binding(binding_id);
      return expr_parser(new_ctx) | [new_ctx, binding_id](ast::Expr body) {
        return ast::TVLambda{binding_id, std::make_unique<ast::Expr>(std::move(body))};
      };
    };
  }();

  auto case_parser = [ctx, rec_expr_parser] -> Parser<ast::Case> {
    auto construct_case = [](ast::Expr scrutinee, std::vector<ast::Case::Choice> choices) {
      return ast::Case{std::make_unique<ast::Expr>(std::move(scrutinee)), std::move(choices)};
    };
    return seq(construct_case, rec_expr_parser(), many(case_choice_parser(ctx)));
  }();

  auto constructor_parser = [ctx, rec_expr_parser](ast::TagId tag_id) -> Parser<ast::Constructor> {
    auto construct_tag = [tag_id](ast::TypeId type_id, std::optional<ast::Expr> argument) {
      auto allocate = [](ast::Expr expr) { return std::make_unique<ast::Expr>(std::move(expr)); };
      return ast::Constructor{tag_id, type_id, std::move(argument).transform(allocate)};
    };
    return seq(construct_tag, type_parser(ctx), optional(rec_expr_parser()));
  };

  return any(std::array{
      atom_exact("lambda") > cut(std::move(lambda_parser)) | to<ast::Expr>,
      atom_exact("tv-lambda") > cut(std::move(tv_lambda_parser)) | to<ast::Expr>,
      atom_exact("case") > cut(std::move(case_parser)) | to<ast::Expr>,
      tag_parser(ctx) >> constructor_parser | to<ast::Expr>,
  });
}

Parser<ast::Expr> expr_parser(Context ctx) noexcept {
  auto rec_expr_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return expr_parser(ctx); }); };

  // TODO: This is already defined in type_parser.
  auto name_defined = [ctx](raw_ast::Atom const &atom) { return !!ctx.lookup(atom.name); };
  auto defined_name = [&] {
    return atom_where(parsy::MeaningfulPredicate{
        .meaning = "a defined name",
        .fn = name_defined,
    });
  };

  auto to_entity_reference = [ctx](ExprView name_view) -> ast::Expr {
    auto entity_id = ctx.lookup(name_view.head_as_atom().name);
    if (not entity_id) {
      todo();
    }
    Context(ctx).capture(*entity_id);
    return ast::EntityReference(*entity_id);
  };

  auto name_lookup = [=] { return defined_name() | to_entity_reference; };

  auto call_parser = [ctx](ast::Expr callee) -> Parser<ast::Expr> {
    auto to_call = [](ast::Expr callee, std::vector<ast::Expr> arguments) -> ast::Expr {
      return ast::Call{std::make_unique<ast::Expr>(std::move(callee)), std::move(arguments)};
    };
    return seq(to_call, pure_once(std::move(callee)), cut(some(expr_parser(ctx))));
  };

  return any(std::array{
      name_lookup(),
      list(cut(any(std::array{
          name_lookup() >> call_parser,
          special_parser(ctx),
          list(cut(rec_expr_parser())) >> call_parser,
      }))),
  });
}

template <typename T> Parser<shallow_ast::ShallowEntity> name_and_body_parser() noexcept {
  auto transform = [](ExprView name_view, ExprView body) -> shallow_ast::ShallowEntity {
    return T(std::move(name_view.head_as_atom().name), std::move(body.head()));
  };

  auto name = [] {
    return atom_where(parsy::MeaningfulPredicate{
        .meaning = "a name",
        .fn = [](auto &) { return true; },
    });
  };

  auto body = [] {
    return satisfies<ExprView>(parsy::MeaningfulPredicate{
        .meaning = "a body",
        .fn = [](auto &) { return true; },
    });
  };

  return seq(transform, name(), body());
}

Parser<shallow_ast::ShallowEntity> shallow_entity_parser() noexcept {
  return list(any(std::array{
      atom_exact("dec") > name_and_body_parser<shallow_ast::ShallowValueDeclaration>(),
      atom_exact("def") > cut(name_and_body_parser<shallow_ast::ShallowValueDefinition>()),
      atom_exact("form") > name_and_body_parser<shallow_ast::ShallowTypeFormDefinition>(),
      atom_exact("module") > name_and_body_parser<shallow_ast::ShallowModuleDefinition>(),
  }));
}

Parser<std::vector<shallow_ast::ShallowEntity>> shallow_entities_parser() noexcept {
  return list(many(shallow_entity_parser()));
}

} // namespace raw

namespace shallow {

template <typename A>
std::expected<A, parsy::ParseError<ExprView>> parse(raw::Parser<A> const &parser,
                                                    raw_ast::Expr expr) noexcept {
  return parsy::parse(parser, ExprView{std::span(&expr, &expr + 1), {}});
}

std::expected<ast::ValueDefinition, parsy::ParseError<ExprView>>
lower_entity(Context const &ctx, shallow_ast::ShallowValueDefinition value) {
  auto expr = parse(raw::expr_parser(ctx), std::move(value.raw_value));
  if (not expr) {
    return std::unexpected(std::move(expr.error()));
  }
  return ast::ValueDefinition{std::move(value.name), *std::move(expr)};
}

std::expected<ast::TypeFormDefinition, parsy::ParseError<ExprView>>
lower_entity(Context ctx, shallow_ast::ShallowTypeFormDefinition shallow_type_form) {
  auto type = parse(raw::type_parser(std::move(ctx)), std::move(shallow_type_form.raw_type));
  if (not type) {
    todo();
  }
  return ast::TypeFormDefinition{std::move(shallow_type_form.name), *std::move(type)};
}

std::expected<ast::ValueDeclaration, parsy::ParseError<ExprView>>
lower_entity(Context ctx, shallow_ast::ShallowValueDeclaration shallow_value_declaration) {
  auto type_signature = parse(raw::type_parser(std::move(ctx)),
                              std::move(shallow_value_declaration.raw_type_signature));
  if (not type_signature) {
    todo();
  }
  return ast::ValueDeclaration{std::move(shallow_value_declaration.name),
                               *std::move(type_signature)};
}

std::expected<ast::MergedValueDefinition, parsy::ParseError<ExprView>>
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

std::expected<ast::ModuleDefinition, parsy::ParseError<ExprView>>
lower_entity(Context ctx, shallow_ast::ShallowModuleDefinition shallow_module) {
  auto shallow_entities_result =
      parse(raw::shallow_entities_parser(), std::move(shallow_module.raw_entities));
  if (not shallow_entities_result) {
    return std::unexpected(shallow_entities_result.error());
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
      entity_ids.insert({name, {i, ctx.es.reserve(shallow_entity.index())}});
    }
  }

  for (auto &[_, index_and_entity_id] : entity_ids) {
    auto [i, id] = index_and_entity_id;
    auto visitor = [&](auto shallow_entity) -> std::expected<void, parsy::ParseError<ExprView>> {
      // TODO: Figure out a way to remove the copy here.
      std::unordered_map<std::string, ast::EntityId> haha;
      for (auto &[k, v] : entity_ids) {
        haha[k] = v.second;
      }

      auto result =
          shallow::lower_entity(ctx.with_names(std::move(haha)), std::move(shallow_entity));
      if (not result) {
        return std::unexpected(std::move(result.error()));
      }
      ctx.es.store(id, *std::move(result));
      return {};
    };
    auto result = std::visit(visitor, std::move(shallow_entities[i]));
    if (not result) {
      return std::unexpected(std::move(result.error()));
    }
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

std::expected<storage::ResolvedAST, parsy::ParseError<ExprView>>
lower_ast(std::string filename, std::vector<raw_ast::Expr> ast) noexcept {
  auto ts = std::make_unique<storage::TypeStorage>();
  storage::EntityStorage storage;
  auto module_definition = shallow::lower_entity(
      Context(*ts, storage),
      shallow_ast::ShallowModuleDefinition{
          filename,
          raw_ast::Expr(raw_ast::List(std::move(ast)), raw_ast::SourceRange{}),
      });
  if (not module_definition) {
    return std::unexpected(module_definition.error());
  }
  return storage::ResolvedAST{*std::move(module_definition), std::move(ts), storage.produce()};
}

} // namespace parser

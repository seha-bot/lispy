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

Parser<ast::TypeId> type_parser(Context ctx) noexcept {
  auto rec_type_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return type_parser(ctx); }); };

  auto name_defined = [ctx](raw_ast::Atom const &atom) { return !!ctx.lookup(atom.name); };
  auto defined_name = [&] {
    return atom_where(parsy::MeaningfulPredicate{
        .meaning = "a defined type name",
        .fn = name_defined,
    });
  };

  auto lookup = [ctx](ExprView atom_view) -> ast::NamedType {
    auto type_entity_id = ctx.lookup(atom_view.head_as_atom().name);
    if (not type_entity_id) {
      todo();
    }

    // TODO: make sure this actually is a type
    // auto *type_entity = storage.get_if_type_form_definition(*type_entity_id);
    // if (not type_entity) {
    //     todo();
    // }
    return ast::NamedType{*type_entity_id};
  };

  auto store = [ctx](ast::Type type) { return ctx.ts.store(std::move(type)); };

  auto tuple_parser = many(rec_type_parser()) | to<ast::TypeTuple> | store;

  auto variant_parser = [&] {
    auto to_label = [ctx](ExprView atom_view) -> ast::LabelId {
      // TODO: what if this isn't a label?
      return ctx.es.get_label(std::move(atom_view.head_as_atom().name));
    };

    auto element_parser = any(std::array{
        seq(to<ast::TypeVariant::Element>, atom("a variant element name") | to_label,
            pure(std::optional<ast::TypeId>())),
        list(seq(to<ast::TypeVariant::Element>, atom("a variant element name") | to_label,
                 rec_type_parser())),
    });

    return many(std::move(element_parser)) | to<ast::TypeVariant> | store;
  }();

  auto to_parser = seq(to<ast::TypeArrow>, rec_type_parser(), rec_type_parser()) | store;

  return any(std::array{
      defined_name() | lookup | store,
      list(any(std::array{
          atom_exact("tuple") > std::move(tuple_parser),
          atom_exact("variant") > std::move(variant_parser),
          atom_exact("to") > std::move(to_parser),
      })),
  });
}

Parser<ast::Expr> expr_parser(Context ctx) noexcept;

Parser<ast::Case::Choice> case_choice_parser(Context ctx) noexcept {
  auto pattern_label = [] { return atom_starting_with(':'); };
  auto pattern_bindings = [] {
    return many(atom("a pattern binding") < peek(atom("a pattern binding")));
  };
  auto arm = [](Context new_ctx) { return expr_parser(new_ctx); };

  return list(pattern_label() >> [=](ExprView label_name_view) {
    auto label_id = ctx.es.get_label(std::move(label_name_view.head_as_atom().name));

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
                 pure_once(ast::Case::Pattern(label_id, std::move(binding_ids))), arm(new_ctx));
    };
  });
}

Parser<ast::Expr> special_parser(Context ctx) noexcept {
  auto lambda_parser = [ctx](ExprView) -> Parser<ast::Lambda> {
    auto to_parameter = [ctx](ExprView name_view, std::optional<ast::TypeId> type) {
      auto &atom = name_view.head_as_atom();

      auto binding_id = ctx.es.reserve_store(ast::Binding{atom.name, type});
      return std::make_pair(ctx.with_names({{std::move(atom.name), binding_id}}), binding_id);
    };

    auto raw_parameter_parser = any(std::array{
        atom("a parameter name") |
            [=](ExprView name_view) { return to_parameter(name_view, std::nullopt); },
        list(cut(seq(to_parameter, atom("a parameter name"), type_parser(ctx)))),
    });

    return cut(std::move(raw_parameter_parser) >> [](std::pair<Context, ast::EntityId> p) {
      auto [new_ctx, parameter_id] = p;
      return expr_parser(new_ctx) | [new_ctx, parameter_id](ast::Expr body) {
        std::vector<ast::EntityId> captures(new_ctx.captures().begin(), new_ctx.captures().end());
        return ast::Lambda{std::move(captures), parameter_id,
                           std::make_unique<ast::Expr>(std::move(body))};
      };
    });
  };

  auto case_parser = [ctx](ExprView) -> Parser<ast::Case> {
    auto construct_case = [](ast::Expr scrutinee, std::vector<ast::Case::Choice> choices) {
      return ast::Case{std::make_unique<ast::Expr>(std::move(scrutinee)), std::move(choices)};
    };

    return seq(construct_case, expr_parser(ctx), many(case_choice_parser(ctx)));
  };

  auto label_parser = [ctx](ExprView name_view) -> Parser<ast::Constructor> {
    auto label = ctx.es.get_label(std::move(name_view.head_as_atom().name));

    auto construct_label = [label](ast::TypeId type, std::optional<ast::Expr> argument) {
      auto allocate = [](ast::Expr expr) { return std::make_unique<ast::Expr>(std::move(expr)); };
      return ast::Constructor{label, type, std::move(argument).transform(allocate)};
    };

    return seq(construct_label, type_parser(ctx), optional(expr_parser(ctx)));
  };

  return any(std::array{
      atom_exact("lambda") >> lambda_parser | to<ast::Expr>,
      atom_exact("case") >> case_parser | to<ast::Expr>,
      atom_starting_with(':') >> label_parser | to<ast::Expr>,
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
      entity_ids.insert({name, {i, ctx.es.reserve()}});
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

// FIX: this doesn't belong here!!!
bool ast::TypeId::operator==(TypeId const &that) const { return m_rep->equal(*this, that); }
std::string ast::TypeId::to_string() const {
  return std::to_string(m_rep->representative(*this).m_id);
}

module;

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

export module lowerer;

import ast;
import context;
import entity;
import entity_storage;
import parsy;
import raw_ast;
import resolved;
import scope;
import shallow_ast;
import tag;
import tag_storage;
import todo;
import type;
import type_storage;

namespace parser {

namespace {

struct ExprView {
  ExprView(std::span<raw_ast::Expr> view, raw_ast::SourceLocation last_seen)
      : m_view(view), m_range(last_seen, last_seen) {
    if (not view.empty()) {
      m_range = view.front().source_range();
    }
  }

  raw_ast::SourceRange pos() const { return m_range; }
  bool empty() const { return m_view.empty(); }
  ExprView take(std::size_t n) const { return {m_view.subspan(0, n), m_range.first}; }
  raw_ast::Expr &head() { return m_view[0]; }
  raw_ast::Expr const &head() const { return m_view[0]; }

  // TODO: This works, but the m_range manipulation could end up somewhere outside of your code.
  // Example:
  // (def id
  // (lambda (x
  // )x))
  // The error will complain about line 2 right after x (missing type) and while that is correct,
  // it is not inside your code. You could make it point at the closing parenthesis if you want
  // or do something else.
  ExprView next() const { return {m_view.subspan(1), {m_range.last.line, m_range.last.col + 1}}; }

  raw_ast::Atom &head_as_atom() { return *std::get_if<raw_ast::Atom>(&head()); }

private:
  std::span<raw_ast::Expr> m_view;
  raw_ast::SourceRange m_range;
};

auto const alloc = []<typename T>(T val) { return std::make_unique<T>(std::move(val)); };

namespace raw {

template <typename T>
auto const to = []<typename... Ts>(Ts &&...args) { return T(std::forward<Ts>(args)...); };

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

Parser<tag::Id> tag_parser(Context ctx) {
  return atom_starting_with(':') | [ctx = std::move(ctx)](ExprView atom_view) -> tag::Id {
    return ctx.tags.get_tag(std::move(atom_view.head_as_atom().name));
  };
}

struct TypeLookupVisitor {
  type::Type operator()(scope::TypeBinding type_binding) {
    return type::DeBruijnIndex{
        ctx.type_binding_relative_index(type_binding.absolute_index),
    };
  }
  type::Type operator()(scope::TypeFormDefinition type_form_definition) {
    return type::NamedTypeReference{type_form_definition.id};
  }
  type::Type operator()(auto const &) { todo(); }

  Context const &ctx;
};

Parser<type::Id> type_parser(Context ctx) noexcept {
  auto rec_type_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return type_parser(ctx); }); };

  auto name_defined = [ctx](raw_ast::Atom const &atom) { return !!ctx.scope().lookup(atom.name); };
  auto defined_name = [name_defined] {
    return atom_where(parsy::MeaningfulPredicate{
        .meaning = "a defined type name",
        .fn = name_defined,
    });
  };

  auto lookup = [ctx](ExprView name_view) -> type::Type {
    auto scope_entry = ctx.scope().lookup(name_view.head_as_atom().name);
    if (not scope_entry) {
      todo();
    }
    return std::visit(TypeLookupVisitor{ctx}, *scope_entry);
  };

  auto arrow_parser = seq(to<type::Arrow>, rec_type_parser(), rec_type_parser());

  auto forall_parser = [ctx] -> Parser<type::ForAll> {
    // TODO: Factor this out because it's also used in expr_parser in 2 places.
    auto to_binding = [ctx](ExprView name_view) {
      auto &atom = name_view.head_as_atom();
      auto new_ctx = ctx;
      scope::TypeBinding type_binding{new_ctx.push_type_binding()};
      return new_ctx.with_names({{atom.name, type_binding}});
    };
    auto binding_parser = atom("a type binding name") | to_binding;
    return std::move(binding_parser) >> type_parser | to<type::ForAll>;
  }();

  auto variant_parser = [ctx, rec_type_parser] -> Parser<type::Variant> {
    auto element_parser = any(std::array{
        seq(to<type::Element>, tag_parser(ctx), pure(type::Id::unit_id)),
        list(seq(to<type::Element>, tag_parser(ctx), rec_type_parser())),
    });

    return many(std::move(element_parser)) | [](std::vector<type::Element> elements) {
      std::ranges::sort(elements, {}, [](auto &e) { return e.tag_id; });
      return elements;
    } | to<type::Variant>;
  }();

  // FIX: Sort elements by tag_id. There is common functionality in variant_parser,
  // so factor it out.
  auto struct_parser =
      many(list(seq(to<type::Element>, tag_parser(ctx), rec_type_parser()))) | to<type::Struct>;

  auto store = [ctx](type::Type type) { return ctx.ts.store(std::move(type)); };

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

Parser<ast::expr::Expr> expr_parser(Context ctx) noexcept;

Parser<ast::expr::Case::Choice> case_choice_parser(Context const &ctx) noexcept {
  auto pattern_bindings = [] {
    return many(atom("a pattern binding") < peek(atom("a pattern binding")));
  };

  return list(tag_parser(ctx) >> [=](tag::Id tag_id) {
    return pattern_bindings() >> [=](std::vector<ExprView> binding_name_views) {
      std::vector<ast::entity::Binding> bindings;
      bindings.reserve(binding_name_views.size());
      for (auto &binding_name_view : binding_name_views) {
        auto binding_name = std::move(binding_name_view.head_as_atom().name);
        bindings.push_back(ast::entity::Binding{std::move(binding_name), std::nullopt});
      }

      std::unordered_map<std::string, scope::Entry> names;
      names.reserve(binding_name_views.size());
      for (auto &binding : bindings) {
        names.insert({binding.name, scope::Binding{&binding}});
      }
      auto new_ctx = ctx.with_names(std::move(names));

      return seq(to<ast::expr::Case::Choice>,
                 pure_once(ast::expr::Case::Pattern(tag_id, std::move(bindings))),
                 expr_parser(std::move(new_ctx)));
    };
  });
}

Parser<ast::expr::Expr> special_parser(Context const &ctx) noexcept {
  auto rec_expr_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return expr_parser(ctx); }); };

  auto lambda_parser = [ctx] -> Parser<ast::expr::Lambda> {
    auto to_binding = [ctx](ExprView name_view, std::optional<type::Id> type) {
      auto &atom = name_view.head_as_atom();
      auto binding = alloc(ast::entity::Binding{atom.name, type});
      auto new_ctx = ctx.with_names({
          {std::move(atom.name), scope::Binding{binding.get()}},
      });
      return std::make_pair(std::move(new_ctx), std::move(binding));
    };

    auto binding_parser = any(std::array{
        atom("a binding name") |
            [=](ExprView name_view) { return to_binding(name_view, std::nullopt); },
        list(cut(seq(to_binding, atom("a binding name"), type_parser(ctx)))),
    });

    return std::move(binding_parser) >>
           [](std::pair<Context, std::unique_ptr<ast::entity::Binding>> p) {
             auto to_lambda = [ctx = p.first](std::unique_ptr<ast::entity::Binding> binding,
                                              ast::expr::Expr body) {
               std::vector<std::reference_wrapper<ast::entity::Binding const>> captures;
               for (auto *binding_ptr : ctx.scope().captures()) {
                 captures.push_back(*binding_ptr);
               }
               return ast::expr::Lambda{
                   std::move(captures),
                   std::move(binding),
                   alloc(std::move(body)),
               };
             };
             auto [new_ctx, binding] = std::move(p);
             return seq(to_lambda, pure_once(std::move(binding)), expr_parser(new_ctx));
           };
  }();

  auto tv_lambda_parser = [ctx] -> Parser<ast::expr::TVLambda> {
    auto to_binding = [ctx](ExprView name_view) {
      auto &atom = name_view.head_as_atom();
      auto new_ctx = ctx;
      scope::TypeBinding type_binding{new_ctx.push_type_binding()};
      return new_ctx.with_names({{atom.name, type_binding}});
    };

    auto binding_parser = atom("a type binding name") | to_binding;
    return std::move(binding_parser) >> expr_parser | alloc | to<ast::expr::TVLambda>;
  }();

  auto case_parser = [ctx, rec_expr_parser] -> Parser<ast::expr::Case> {
    return seq(to<ast::expr::Case>, rec_expr_parser() | alloc, many(case_choice_parser(ctx)));
  }();

  auto pack_parser = [ctx, rec_expr_parser] -> Parser<ast::expr::Pack> {
    return many(list(seq(to<ast::expr::TaggedValue>, tag_parser(ctx), rec_expr_parser() | alloc))) |
           to<ast::expr::Pack>;
  }();

  return any(std::array{
      atom_exact("lambda") > cut(std::move(lambda_parser)) | to<ast::expr::Expr>,
      atom_exact("tv-lambda") > cut(std::move(tv_lambda_parser)) | to<ast::expr::Expr>,
      atom_exact("case") > cut(std::move(case_parser)) | to<ast::expr::Expr>,
      atom_exact("pack") > cut(std::move(pack_parser)) | to<ast::expr::Expr>,
  });
}

struct ValueLookupVisitor {
  ast::expr::Expr operator()(scope::Binding binding) {
    ctx.scope().capture(*binding.binding_ptr);
    return ast::expr::BindingReference{*binding.binding_ptr};
  }
  ast::expr::Expr operator()(scope::ValueDeclaration v) { return ast::expr::ValueReference{v.id}; }
  ast::expr::Expr operator()(scope::ValueDefinition v) { return ast::expr::ValueReference{v.id}; }
  ast::expr::Expr operator()(scope::MergedValueDefinition v) {
    return ast::expr::ValueReference{v.id};
  }
  ast::expr::Expr operator()(auto const &) { todo(); }

  Context const &ctx;
};

Parser<ast::expr::Expr> expr_parser(Context ctx) noexcept {
  auto rec_expr_parser = [ctx] { return parsy::rec<ExprView>([ctx] { return expr_parser(ctx); }); };

  // TODO: This is already defined in type_parser.
  auto name_defined = [ctx](raw_ast::Atom const &atom) { return !!ctx.scope().lookup(atom.name); };
  auto defined_name = [&] {
    return atom_where(parsy::MeaningfulPredicate{
        .meaning = "a defined name",
        .fn = name_defined,
    });
  };

  auto to_entity_reference = [ctx](ExprView name_view) -> ast::expr::Expr {
    auto scope_entry = ctx.scope().lookup(name_view.head_as_atom().name);
    if (not scope_entry) {
      todo();
    }
    return std::visit(ValueLookupVisitor{ctx}, *scope_entry);
  };

  auto name_lookup = [=] { return defined_name() | to_entity_reference; };

  auto call_parser = [ctx](ast::expr::Expr function) -> Parser<ast::expr::Application> {
    auto flatten = [](ast::expr::Expr function, std::vector<ast::expr::Expr> arguments) {
      ast::expr::Application app{alloc(std::move(function)), alloc(std::move(arguments.front()))};
      for (std::size_t i = 1; i < arguments.size(); ++i) {
        app = ast::expr::Application{
            alloc(ast::expr::Expr{std::move(app)}),
            alloc(std::move(arguments[i])),
        };
      }
      return app;
    };

    return seq(flatten, pure_once(std::move(function)), cut(some(expr_parser(ctx))));
  };

  return any(std::array{
      name_lookup(),
      seq(to<ast::expr::Variant>, tag_parser(ctx), pure(std::nullopt)) | to<ast::expr::Expr>,
      list(cut(any(std::array{
          name_lookup() >> call_parser | to<ast::expr::Expr>,
          seq(to<ast::expr::Variant>, tag_parser(ctx), rec_expr_parser() | alloc) |
              to<ast::expr::Expr>,
          special_parser(ctx),
          list(cut(rec_expr_parser())) >> call_parser | to<ast::expr::Expr>,
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

std::expected<ast::entity::ValueDeclaration, parsy::ParseError<ExprView>>
lower_entity(Context ctx, shallow_ast::ShallowValueDeclaration shallow_value_declaration) {
  auto type_signature = parse(raw::type_parser(std::move(ctx)),
                              std::move(shallow_value_declaration.raw_type_signature));
  if (not type_signature) {
    todo();
  }
  return ast::entity::ValueDeclaration{
      std::move(shallow_value_declaration.name),
      *std::move(type_signature),
  };
}

std::expected<ast::entity::ValueDefinition, parsy::ParseError<ExprView>>
lower_entity(Context const &ctx, shallow_ast::ShallowValueDefinition value) {
  auto expr = parse(raw::expr_parser(ctx), std::move(value.raw_value));
  if (not expr) {
    return std::unexpected(std::move(expr.error()));
  }
  return ast::entity::ValueDefinition{std::move(value.name), alloc(*std::move(expr))};
}

std::expected<ast::entity::MergedValueDefinition, parsy::ParseError<ExprView>>
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

  return ast::entity::MergedValueDefinition{
      std::move(shallow_merged_value_definition.name),
      *std::move(type_signature),
      alloc(*std::move(expr)),
  };
}

std::expected<ast::entity::TypeFormDefinition, parsy::ParseError<ExprView>>
lower_entity(Context ctx, shallow_ast::ShallowTypeFormDefinition shallow_type_form) {
  auto type = parse(raw::type_parser(std::move(ctx)), std::move(shallow_type_form.raw_type));
  if (not type) {
    todo();
  }
  return ast::entity::TypeFormDefinition{std::move(shallow_type_form.name), *std::move(type)};
}

std::expected<ast::entity::ModuleDefinition, parsy::ParseError<ExprView>>
lower_entity(Context ctx, shallow_ast::ShallowModuleDefinition shallow_module) {
  auto shallow_entities_result =
      parse(raw::shallow_entities_parser(), std::move(shallow_module.raw_entities));
  if (not shallow_entities_result) {
    return std::unexpected(shallow_entities_result.error());
  }

  auto &shallow_entities = *shallow_entities_result;
  std::unordered_map<std::string, std::pair<std::size_t, entity::Id>> entity_ids;
  std::unordered_map<std::string, scope::Entry> names;

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
      struct Visitor {
        scope::Entry operator()(shallow_ast::ShallowValueDeclaration const &) {
          return scope::ValueDeclaration{id};
        }
        scope::Entry operator()(shallow_ast::ShallowValueDefinition const &) {
          return scope::ValueDefinition{id};
        }
        scope::Entry operator()(shallow_ast::ShallowMergedValueDefinition const &) {
          return scope::MergedValueDefinition{id};
        }
        scope::Entry operator()(shallow_ast::ShallowTypeFormDefinition const &) {
          return scope::TypeFormDefinition{id};
        }
        scope::Entry operator()(shallow_ast::ShallowModuleDefinition const &) { todo(); }

        entity::Id id;
      };
      auto id = ctx.es.reserve();
      entity_ids.insert({name, {i, id}});
      names.insert({name, std::visit(Visitor{id}, shallow_entity)});
    }
  }
  auto new_ctx = ctx.with_names(std::move(names));

  for (auto &[_, index_and_id] : entity_ids) {
    auto [i, id] = index_and_id;
    auto visitor = [&](auto shallow_entity) -> std::expected<void, parsy::ParseError<ExprView>> {
      auto result = shallow::lower_entity(new_ctx, std::move(shallow_entity));
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

  return ast::entity::ModuleDefinition{std::move(shallow_module.name)};
}

} // namespace shallow

} // namespace

export std::expected<storage::ResolvedAST, parsy::ParseError<ExprView>>
lower_ast(std::string filename, std::vector<raw_ast::Expr> ast) noexcept {
  auto ts = std::make_unique<storage::TypeStorage>();
  EntityStorage es;
  TagStorage tags;
  auto module_definition = shallow::lower_entity(
      Context(*ts, es, tags),
      shallow_ast::ShallowModuleDefinition{
          std::move(filename),
          raw_ast::Expr(raw_ast::List(std::move(ast)), raw_ast::SourceRange{}),
      });
  if (not module_definition) {
    return std::unexpected(module_definition.error());
  }
  auto e = std::move(es).finalize();
  auto t = std::move(tags).finalize();
  return storage::ResolvedAST{
      *std::move(module_definition),
      std::move(ts),
      std::move(e),
      std::move(t),
  };
}

} // namespace parser

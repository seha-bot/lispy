#include "raw.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "context.hpp"
#include "pars.hpp"
#include "todo.hpp"

namespace parser::raw {

using namespace pars;

Parser<ast::TypeId> type_parser(Context ctx) noexcept {
  auto name_defined = [ctx](std::string_view name) { return !!ctx.lookup(name); };

  auto lookup = [ctx](std::string const &name) {
    auto type_entity_id = ctx.lookup(name);
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
  auto rec_type_parser = [ctx] { return rec([ctx] { return type_parser(ctx); }); };

  auto tuple_parser = many(rec_type_parser()) | to<ast::TypeTuple> | store;

  auto variant_parser = [&] {
    auto to_label = [ctx](std::string name) {
      // TODO: what if this isn't a label?
      return ctx.es.get_label(std::move(name));
    };

    auto element_parser = ANY{
        seq(to<ast::TypeVariant::Element>, atom() | to_label, pure(std::optional<ast::TypeId>())),
        list(seq(to<ast::TypeVariant::Element>, atom() | to_label, rec_type_parser())),
    };

    return many(std::move(element_parser)) | to<ast::TypeVariant> | store;
  }();

  auto to_parser = seq(to<ast::TypeArrow>, rec_type_parser(), rec_type_parser()) | store;

  return ANY{
      atom_where(name_defined) | lookup | store,
      list(ANY{
          atom_exact("tuple") > std::move(tuple_parser),
          atom_exact("variant") > std::move(variant_parser),
          atom_exact("to") > std::move(to_parser),
      }),
  };
}

Parser<ast::Case::Alternative> case_arm_parser(Context ctx) noexcept {
  auto atom_pat = [ctx](std::string name) -> ast::Case::Pattern {
    auto label_id = ctx.es.get_label(std::move(name));
    return ast::Case::Pattern(label_id, std::nullopt);
  };

  auto list_pat = [ctx](std::string name, std::string binding_name) {
    auto label_id = ctx.es.get_label(std::move(name));
    auto binding_id = ctx.es.reserve_store(ast::Binding{binding_name, std::nullopt});
    auto new_ctx = ctx.with_names({{std::move(binding_name), binding_id}});
    return std::make_pair(new_ctx, ast::Case::Pattern(label_id, binding_id));
  };

  auto list_arm = [](std::pair<Context, ast::Case::Pattern> p) -> Parser<ast::Case::Alternative> {
    auto [new_ctx, pat] = p;
    return expr_parser(new_ctx) |
           [pat](ast::Expr expr) { return ast::Case::Alternative(pat, std::move(expr)); };
  };

  return ANY{
      seq(to<ast::Case::Alternative>, atom_starting_with(':') | atom_pat, expr_parser(ctx)),
      list(seq(list_pat, atom_starting_with(':'), atom())) >> list_arm,
  };
}

Parser<ast::Expr> special_parser(Context ctx) noexcept {
  auto lambda_parser = [ctx](std::string const &) -> Parser<ast::Lambda> {
    auto to_parameter = [ctx](std::string name, std::optional<ast::TypeId> type) {
      auto binding_id = ctx.es.reserve_store(ast::Binding{name, type});
      return std::make_pair(ctx.with_names({{std::move(name), binding_id}}), binding_id);
    };

    auto raw_parameter_parser = ANY{
        atom() | [=](std::string name) { return to_parameter(std::move(name), std::nullopt); },
        list(seq(to_parameter, atom(), optional(type_parser(ctx)))),
    };

    return std::move(raw_parameter_parser) >> [](std::pair<Context, ast::EntityId> p) {
      auto [new_ctx, parameter_id] = p;
      return expr_parser(new_ctx) | [new_ctx, parameter_id](ast::Expr body) {
        std::vector<ast::EntityId> captures(new_ctx.captures().begin(), new_ctx.captures().end());
        return ast::Lambda{std::move(captures), parameter_id,
                           std::make_unique<ast::Expr>(std::move(body))};
      };
    };
  };

  auto case_parser = [ctx](std::string const &) -> Parser<ast::Case> {
    auto construct_case = [](ast::Expr scrutinee,
                             std::vector<ast::Case::Alternative> alternatives) {
      return ast::Case{std::make_unique<ast::Expr>(std::move(scrutinee)), std::move(alternatives)};
    };

    return seq(construct_case, expr_parser(ctx), many(case_arm_parser(ctx)));
  };

  auto label_parser = [ctx](std::string name) -> Parser<ast::Constructor> {
    auto label = ctx.es.get_label(std::move(name));

    auto construct_label = [label](ast::TypeId type, std::optional<ast::Expr> argument) {
      auto allocate = [](ast::Expr expr) { return std::make_unique<ast::Expr>(std::move(expr)); };
      return ast::Constructor{label, type, std::move(argument).transform(allocate)};
    };

    return seq(construct_label, type_parser(ctx), optional(expr_parser(ctx)));
  };

  return UNIFY(ast::Expr)({
      atom_exact("lambda") >> lambda_parser,
      atom_exact("case") >> case_parser,
      atom_starting_with(':') >> label_parser,
  });
}

Parser<ast::Expr> expr_parser(Context ctx) noexcept {
  auto name_defined = [ctx](std::string_view name) { return !!ctx.lookup(name); };

  auto to_entity_reference = [ctx](std::string const &name) mutable -> ast::Expr {
    auto entity_id = ctx.lookup(name);
    if (not entity_id) {
      todo();
    }
    ctx.capture(*entity_id);
    return ast::EntityReference(*entity_id);
  };

  auto name_lookup = [=] { return atom_where(name_defined) | to_entity_reference; };

  auto call_parser = [ctx](ast::Expr callee) -> Parser<ast::Expr> {
    return many(expr_parser(ctx)) |
               [callee = std::move(callee)](std::vector<ast::Expr> arguments) mutable -> ast::Expr {
      return ast::Call{std::make_unique<ast::Expr>(std::move(callee)), std::move(arguments)};
    };
  };

  return ANY{
      name_lookup(),
      list(ANY{
          name_lookup() >> call_parser,
          special_parser(ctx),
          list(rec([ctx] { return expr_parser(ctx); })) >> call_parser,
      }),
  };
}

namespace {

template <typename T> Parser<T> name_and_body_parser() noexcept {
  return seq(to<T>, atom(), pars::raw());
}

Parser<ast::ShallowEntity> shallow_entity_parser() noexcept {
  return UNIFY(ast::ShallowEntity)({
      atom_exact("dec") > name_and_body_parser<ast::ShallowValueDefinition>(),
      atom_exact("def") > name_and_body_parser<ast::ShallowValueDeclaration>(),
      atom_exact("form") > name_and_body_parser<ast::ShallowTypeFormDefinition>(),
      atom_exact("module") > name_and_body_parser<ast::ShallowModuleDefinition>(),
  });
}

} // namespace

Parser<std::vector<ast::ShallowEntity>> shallow_entities_parser() noexcept {
  return many(shallow_entity_parser());
}

} // namespace parser::raw

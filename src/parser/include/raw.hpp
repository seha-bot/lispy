#ifndef RAW_HPP
#define RAW_HPP

#include "context.hpp"
#include "pars.hpp"
#include "shallow_ast.hpp"

namespace parser::raw {

pars::Parser<ast::TypeId> type_parser(Context ctx) noexcept;
pars::Parser<ast::Case::Choice> case_choice_parser(Context ctx) noexcept;
pars::Parser<ast::Expr> special_parser(Context ctx) noexcept;
pars::Parser<ast::Expr> expr_parser(Context ctx) noexcept;
pars::Parser<std::vector<shallow_ast::ShallowEntity>> shallow_entities_parser() noexcept;

} // namespace parser::raw

#endif

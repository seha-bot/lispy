#ifndef RAW_HPP
#define RAW_HPP

#include "context.hpp"
#include "pars.hpp"

namespace parser::raw {

pars::Parser<ast::TypeId> type_parser(Context ctx) noexcept;
pars::Parser<ast::Case::Alternative> case_arm_parser(Context ctx) noexcept;
pars::Parser<ast::Expr> special_parser(Context ctx) noexcept;
pars::Parser<ast::Expr> expr_parser(Context ctx) noexcept;
pars::Parser<ast::ShallowEntity> module_entity_parser() noexcept;

} // namespace parser::raw

#endif

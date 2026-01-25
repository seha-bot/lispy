#ifndef LIS_PARSER_HPP
#define LIS_PARSER_HPP

#include <cctype>
#include <optional>
#include <string>
#include <vector>

#include "ast.hpp"
#include "gc.hpp"
#include "parser.hpp"

namespace parse {

Parser<char> atom_part() {
    return satisfy([](char c) { return not std::isspace(c) and c != '(' and c != ')' and c != '\''; });
}

Parser<ast::Expr *> atom(GC& gc) {
    return atom_part().some().map(
        [&gc](std::vector<char> const& v) -> ast::Expr * { return gc.alloc_atom(std::string(v.begin(), v.end())); });
}

auto recurse(auto f) {
    return pure(0) + [f](int) { return f(); };
}

Parser<std::optional<char>> maybe_quote() {
    return char_('\'').map([](char c) { return std::make_optional(c); }) or pure(std::optional<char>());
}

Parser<ast::Expr *> s_expr(GC& gc) {
    return ws() >
           sequence(maybe_quote(),
                    atom(gc) or
                        char_('(') >
                            recurse([&gc] { return s_expr(gc); }).many().map([&gc](std::vector<ast::Expr *> const& v) {
                                ast::Expr *acc = gc.nil();
                                for (auto it = v.rbegin(); it != v.rend(); ++it) {
                                    acc = gc.alloc_cons(*it, acc);
                                }
                                return acc;
                            }) < char_(')'))
               .map([&gc](std::tuple<std::optional<char>, ast::Expr *> x) -> ast::Expr * {
                   if (std::get<0>(x)) {
                       return gc.alloc_cons(gc.alloc_atom("QUOTE"), gc.alloc_cons(std::get<1>(x), gc.nil()));
                   } else {
                       return std::get<1>(x);
                   }
               });
}

}  // namespace parse

#endif

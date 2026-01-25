#ifndef LIS_PARSER_HPP
#define LIS_PARSER_HPP

#include <cctype>
#include <optional>
#include <string>
#include <vector>

#include "alloc.hpp"
#include "ast.hpp"
#include "parser.hpp"

namespace parse {

Parser<char> atom_part() {
    return satisfy([](char c) { return not std::isspace(c) and c != '(' and c != ')' and c != '\''; });
}

Parser<ast::Expr *> atom(Alloc& alloc) {
    return atom_part().some().map(
        [&alloc](std::vector<char> const& v) -> ast::Expr * { return alloc.atom(std::string(v.begin(), v.end())); });
}

auto recurse(auto f) {
    return pure(0) + [f](int) { return f(); };
}

Parser<std::optional<char>> maybe_quote() {
    return char_('\'').map([](char c) { return std::make_optional(c); }) or pure(std::optional<char>());
}

Parser<ast::Expr *> s_expr(Alloc& alloc) {
    return ws() > sequence(maybe_quote(),
                           atom(alloc) or char_('(') > recurse([&alloc] { return s_expr(alloc); })
                                                           .many()
                                                           .map([&alloc](std::vector<ast::Expr *> const& v) {
                                                               ast::Expr *acc = alloc.nil();
                                                               for (auto it = v.rbegin(); it != v.rend(); ++it) {
                                                                   acc = alloc.cons(*it, acc);
                                                               }
                                                               return acc;
                                                           }) < ws() < char_(')'))
                      .map([&alloc](std::tuple<std::optional<char>, ast::Expr *> x) -> ast::Expr * {
                          if (std::get<0>(x)) {
                              return alloc.cons(alloc.atom("QUOTE"), alloc.cons(std::get<1>(x), alloc.nil()));
                          } else {
                              return std::get<1>(x);
                          }
                      });
}

}  // namespace parse

#endif

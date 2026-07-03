#ifndef PARS_HPP
#define PARS_HPP

// A collection of parser combinators for parsing RawExprs.

#include "ast.hpp"
#include "todo.hpp"
#include <concepts>
#include <expected>
#include <functional>

namespace parser::pars {

template <typename T>
auto to = []<typename... Ts>(Ts &&...args) { return T(std::forward<Ts>(args)...); };

#define PROPAGATE(x)                                                                               \
  return { std::move(x.error()), x.is_soft() }

struct Input {
  Input(std::span<ast::RawExpr> exprs) : data(exprs) {}

  Input drop(std::size_t n) const {
    if (data.size() < n) {
      todo();
    }
    return data.subspan(n);
  }

  ast::RawExpr &front() {
    if (data.empty()) {
      todo();
    }
    return data.front();
  }

  bool empty() const { return data.empty(); }

private:
  std::span<ast::RawExpr> data;
};

enum class ParseError {
  not_an_atom,
};

namespace errors {

struct NoExpression {};
struct NotAnAtom {};
struct AtomFailedPredicate {};
struct AtomDoesNotMatch {
  std::string_view expected, got;
};

// TODO: remove
struct NothingMatched {};

using ParseErrorBase =
    std::variant<NoExpression, NotAnAtom, AtomFailedPredicate, AtomDoesNotMatch, NothingMatched>;
struct ParseError : ParseErrorBase {
  using ParseErrorBase::variant;
};

} // namespace errors

template <typename T> struct ParseResult {
  ParseResult(T result, Input remaining) : m_result(std::pair{std::move(result), remaining}) {}
  ParseResult(errors::ParseError error, bool is_soft = false)
      : m_result(std::unexpected(std::pair{std::move(error), is_soft})) {}

  explicit operator bool() const { return static_cast<bool>(m_result); }

  T &value() { return m_result.value().first; }
  Input remaining() const { return m_result.value().second; }
  errors::ParseError &error() { return m_result.error().first; }
  bool is_soft() { return m_result.error().second; }

private:
  std::expected<std::pair<T, Input>, std::pair<errors::ParseError, bool>> m_result;
};

template <typename T> struct Parser {
  Parser(std::move_only_function<ParseResult<T>(Input) const> parser)
      : m_parser(std::move(parser)) {}

  ParseResult<T> run(Input expr) const { return m_parser(expr); }

private:
  std::move_only_function<ParseResult<T>(Input) const> m_parser;
};

template <typename T>
std::expected<T, errors::ParseError> parse(Parser<T> parser, ast::RawExpr expr) noexcept {
  std::array exprs{std::move(expr)};
  auto result = parser.run(Input(exprs));
  if (not result) {
    return std::unexpected(std::move(result.error()));
  }
  if (not result.remaining().empty()) {
    todo();
  }
  return std::move(result.value());
}

template <typename Fn> auto rec(Fn fn) -> decltype(std::as_const(fn)()) {
  return {[parser = std::move(fn)](Input exprs) { return parser().run(exprs); }};
}

template <typename T> Parser<T> pure(T value) {
  return {[v = std::move(value)](Input exprs) { return ParseResult<T>{v, std::move(exprs)}; }};
}

template <typename T, typename U, std::invocable<T, U> Fn>
auto seq(Fn f, Parser<T> a, Parser<U> b)
    -> Parser<decltype(f(std::declval<T>(), std::declval<U>()))> {
  return {[f = std::move(f), a = std::move(a), b = std::move(b)](
              Input exprs) -> ParseResult<decltype(f(std::declval<T>(), std::declval<U>()))> {
    auto res_a = a.run(exprs);
    if (not res_a) {
      PROPAGATE(res_a);
    }

    auto res_b = b.run(res_a.remaining());
    if (not res_b) {
      PROPAGATE(res_b);
    }

    return ParseResult{f(std::move(res_a.value()), std::move(res_b.value())), res_b.remaining()};
  }};
}

Parser<std::string> atom_where(std::predicate<std::string_view> auto p) {
  return {[p = std::move(p)](Input exprs) -> ParseResult<std::string> {
    if (exprs.empty()) {
      return {errors::NoExpression{}, true};
    }

    auto *atom_expr = std::get_if<ast::Atom>(&exprs.front());
    if (not atom_expr) {
      return {errors::NotAnAtom{}, true};
    }
    if (not p(std::string_view(atom_expr->name()))) {
      return {errors::AtomFailedPredicate{}, true};
    }

    return {std::move(atom_expr->name()), exprs.drop(1)};
  }};
}

template <typename T> Parser<T> list(Parser<T> parser) {
  return {[parser = std::move(parser)](Input exprs) -> ParseResult<T> {
    if (exprs.empty()) {
      return {errors::NoExpression{}, true};
    }

    auto *list_expr = std::get_if<ast::List>(&exprs.front());
    if (not list_expr) {
      todo();
    }

    auto result = parser.run(Input(list_expr->elements()));
    if (not result) {
      PROPAGATE(result);
    }

    if (not result.remaining().empty()) {
      todo();
    }

    return {std::move(result.value()), exprs.drop(1)};
  }};
}

inline Parser<ast::RawExpr> raw() {
  return {[](Input exprs) -> ParseResult<ast::RawExpr> {
    if (exprs.empty()) {
      todo();
    }
    return {std::move(exprs.front()), exprs.drop(1)};
  }};
}

template <typename T, std::invocable<T> Fn>
auto operator|(Parser<T> parser, Fn f) -> Parser<decltype(f(std::declval<T>()))> {
  return {[parser = std::move(parser),
           f = std::move(f)](Input exprs) -> ParseResult<decltype(f(std::declval<T>()))> {
    auto result = parser.run(exprs);
    if (not result) {
      PROPAGATE(result);
    }
    return ParseResult{f(std::move(result.value())), result.remaining()};
  }};
}

template <typename T, std::invocable<T> Fn>
auto operator>>(Parser<T> parser, Fn f) -> decltype(f(std::declval<T>())) {
  return {[parser = std::move(parser), f = std::move(f)](
              Input exprs) -> decltype(f(std::declval<T>()).run(std::declval<Input>())) {
    auto result = parser.run(exprs);
    if (not result) {
      PROPAGATE(result);
    }
    return f(std::move(result.value())).run(result.remaining());
  }};
}

template <typename T, typename U> Parser<U> operator>(Parser<T> a, Parser<U> b) {
  return {[a = std::move(a), b = std::move(b)](Input exprs) -> ParseResult<U> {
    auto result = a.run(exprs);
    if (not result) {
      PROPAGATE(result);
    }
    return b.run(result.remaining());
  }};
}

template <typename T> Parser<std::optional<T>> optional(Parser<T> parser) {
  return {[parser = std::move(parser)](Input exprs) -> ParseResult<std::optional<T>> {
    auto result = parser.run(exprs);
    if (not result) {
      if (result.is_soft()) {
        return {std::nullopt, std::move(exprs)};
      }
      PROPAGATE(result);
    }
    return {std::move(result.value()), result.remaining()};
  }};
}

template <std::move_constructible T> Parser<T> pure_once(T value) {
  return {[v = std::make_unique<T>(std::move(value))](Input exprs) {
    return ParseResult<T>{std::move(*v), std::move(exprs)};
  }};
}

template <typename T, std::size_t N> Parser<T> any(std::array<Parser<T>, N> parsers) {
  return {[parsers = std::move(parsers)](Input exprs) -> ParseResult<T> {
    for (auto &parser : parsers) {
      auto result = parser.run(exprs);
      if (result) {
        return result;
      }
      if (not result.is_soft()) {
        PROPAGATE(result);
      }
    }

    // TODO: combine errors instead of doing this.
    return {errors::NothingMatched{}, true};
  }};
}

struct AnyTag {
  template <typename T, std::size_t N> Parser<T> operator=(std::array<Parser<T>, N> parsers) {
    return any(std::move(parsers));
  }
};
#define ANY AnyTag{} = std::array

template <typename T> Parser<std::vector<T>> many(Parser<T> parser) {
  return {[parser = std::move(parser)](Input exprs) -> ParseResult<std::vector<T>> {
    std::vector<T> out;
    while (not exprs.empty()) {
      auto res = parser.run(exprs);
      if (not res) {
        if (not res.is_soft()) {
          PROPAGATE(res);
        }
        break;
      }
      out.push_back(std::move(res.value()));
      exprs = res.remaining();
    }
    return ParseResult{std::move(out), exprs};
  }};
}

inline Parser<std::string> atom() {
  return atom_where([](std::string_view) { return true; });
}

inline Parser<std::string> atom_exact(std::string_view value) {
  return atom_where([=](std::string_view name) { return name == value; });
}

inline Parser<std::string> atom_starting_with(char c) {
  return atom_where([c](std::string_view name) { return name.at(0) == c; });
}

template <typename T, typename... Ts> Parser<T> unify(std::tuple<Ts...> parsers) {
  return ANY{(std::move(std::get<Ts>(parsers)) | to<T>)...};
}

#define UNIFY_IMPL(...) (std::tuple __VA_ARGS__)
#define UNIFY(T) unify<T> UNIFY_IMPL

#undef PROPAGATE

} // namespace parser::pars

#endif

module;

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

export module parsy;

export namespace parsy {

template <typename T>
concept TokenView =
    std::is_trivially_copy_constructible_v<T> and requires(T const &ct, std::size_t n) {
      { ct.pos() };
      { ct.empty() } -> std::same_as<bool>;
      { ct.take(n) } -> std::same_as<T>;
      { ct.head() };
      { ct.next() } -> std::same_as<T>;
    };

template <TokenView T> struct TokenTraits {
  using pos = decltype(std::declval<T>().pos());
  using value = decltype(std::declval<T>().head());
};

template <TokenView T> struct ParseError;

namespace errors {

template <TokenView T> struct Combined {
  typename TokenTraits<T>::pos pos;
  std::vector<ParseError<T>> errors;
};

struct Custom {
  std::string msg;
};

template <TokenView T> struct Unexpected {
  typename TokenTraits<T>::pos pos;
  std::string expected;
};

template <TokenView T> using Error = std::variant<Combined<T>, Custom, Unexpected<T>>;

} // namespace errors

template <TokenView T> struct ParseError {
  friend std::ostream &operator<<(std::ostream &os, ParseError const &parse_error) {
    struct Visitor {
      void operator()(errors::Combined<T> const &err) {
        os << "Tried parsing something at " << err.pos
           << ", but failed. Here are the reasons each attempt failed:\n";
        for (auto &e : err.errors) {
          os << e << '\n';
        }
      }
      void operator()(errors::Custom const &err) { os << err.msg; }
      void operator()(errors::Unexpected<T> const &err) {
        os << "Expected " << err.expected << " at " << err.pos << '.';
      }

      std::ostream &os;
    };
    std::visit(Visitor{os}, parse_error.error);
    return os;
  }

  errors::Error<T> error;
  bool is_recoverable;
};

template <TokenView T, typename A> struct ParseResult {
  ParseResult(A result, T remaining) : m_result(std::pair{std::move(result), remaining}) {}
  ParseResult(ParseError<T> error) : m_result(std::unexpected(std::move(error))) {}

  A &value() { return m_result.value().first; }
  T remaining() const { return m_result.value().second; }
  ParseError<T> &error() { return m_result.error(); }

  explicit operator bool() const { return static_cast<bool>(m_result); }

private:
  std::expected<std::pair<A, T>, ParseError<T>> m_result;
};

template <TokenView T, typename A> struct Parsy {
  using Impl = std::move_only_function<ParseResult<T, A>(T) const>;

  Parsy(Impl parser) : m_parser(std::move(parser)) {}
  ParseResult<T, A> run(T tokens) const { return m_parser(tokens); }

private:
  Impl m_parser;
};

namespace detail {

template <TokenView T, typename A> ParseResult<T, A> parse(Parsy<T, A>) { static_assert(false); }

template <typename P, typename T>
concept Parser = TokenView<T> and requires { parse<T>(std::declval<P>()); };

template <typename Fn, typename T, typename... Args>
concept ParserCallback = TokenView<T> and requires(Fn &&fn, Args &&...args) {
  { fn(std::move(args)...) } -> Parser<T>;
};

template <TokenView T, Parser<T> P> using Result = decltype(parse(std::declval<P>()));

template <TokenView T, typename A, typename... As>
ParseResult<T, std::tuple<A, As...>> seq_impl(T tokens, Parsy<T, A> const &parser,
                                              Parsy<T, As> const &...parsers) {
  auto result = parser.run(tokens);
  if (not result) {
    return std::move(result.error());
  }

  if constexpr (sizeof...(As) == 0) {
    return {std::tuple<A>{std::move(result.value())}, result.remaining()};
  } else {
    auto results = seq_impl(result.remaining(), std::move(parsers)...);
    if (not results) {
      return std::move(results.error());
    }

    return std::apply(
        [&](auto &&...results_unwrapped) -> ParseResult<T, std::tuple<A, As...>> {
          return {
              std::tuple<A, As...>{std::move(result.value()), std::move(results_unwrapped)...},
              results.remaining(),
          };
        },
        std::move(results.value()) //
    );
  }
}

} // namespace detail

template <TokenView T, typename A>
std::expected<A, ParseError<T>> parse(Parsy<T, A> const &parser, T tokens) noexcept {
  auto result = parser.run(tokens);
  if (not result) {
    return std::unexpected(std::move(result.error()));
  }
  if (not result.remaining().empty()) {
    return std::unexpected(ParseError<T>{
        .error = errors::Unexpected<T>{.pos = result.remaining().pos(), .expected = "nothing"},
        .is_recoverable = false,
    });
  }
  return std::move(result.value());
}

template <typename Pred> struct MeaningfulPredicate {
  std::string meaning;
  Pred fn;
};

template <TokenView T, typename A, std::invocable<T, errors::Error<T>> Fn>
Parsy<T, A> map_error(Fn fn, Parsy<T, A> parser) {
  return {[fn = std::move(fn), parser = std::move(parser)](T tokens) -> ParseResult<T, A> {
    auto result = parser.run(tokens);
    if (not result) {
      return ParseError<T>{
          .error = fn(tokens, std::move(result.error().error)),
          .is_recoverable = result.error().is_recoverable,
      };
    }
    return result;
  }};
}

template <TokenView T, typename A> Parsy<T, A> cut(Parsy<T, A> parser) {
  return {[parser = std::move(parser)](T tokens) -> ParseResult<T, A> {
    auto result = parser.run(tokens);
    if (not result) {
      return ParseError<T>{
          .error = std::move(result.error().error),
          .is_recoverable = false,
      };
    }
    return result;
  }};
}

template <TokenView T, detail::ParserCallback<T> Fn>
auto rec(Fn fn) -> decltype(std::as_const(fn)()) {
  return {[fn = std::move(fn)](T tokens) { return fn().run(tokens); }};
}

template <TokenView T, typename A> Parsy<T, A> pure(A value) {
  return {[v = std::move(value)](T tokens) -> ParseResult<T, A> { return {v, tokens}; }};
}

// TODO: This is shit.
template <TokenView T, typename A> Parsy<T, A> pure_once(A value) {
  return {[v = std::make_unique<A>(std::move(value))](T tokens) -> ParseResult<T, A> {
    return {std::move(*v), tokens};
  }};
}

template <TokenView T, typename A> Parsy<T, A> peek(Parsy<T, A> parser) {
  return {[parser = std::move(parser)](T tokens) -> ParseResult<T, A> {
    auto result = parser.run(tokens);
    if (not result) {
      return std::move(result.error());
    }
    return {std::move(result.value()), tokens};
  }};
}

template <TokenView T, typename A, std::invocable<A> Fn>
auto operator|(Parsy<T, A> parser, Fn f) -> Parsy<T, decltype(f(std::declval<A>()))> {
  return {[parser = std::move(parser),
           f = std::move(f)](T tokens) -> ParseResult<T, decltype(f(std::declval<A>()))> {
    auto result = parser.run(tokens);
    if (not result) {
      return std::move(result.error());
    }
    return {f(std::move(result.value())), result.remaining()};
  }};
}

template <TokenView T, typename A, detail::ParserCallback<T, A> Fn>
auto operator>>(Parsy<T, A> parser, Fn f) -> decltype(f(std::declval<A>())) {
  return {[parser = std::move(parser),
           f = std::move(f)](T tokens) -> detail::Result<T, decltype(f(std::declval<A>()))> {
    auto result = parser.run(tokens);
    if (not result) {
      return std::move(result.error());
    }
    return f(std::move(result.value())).run(result.remaining());
  }};
}

template <TokenView T, typename A, typename B> Parsy<T, B> operator>(Parsy<T, A> a, Parsy<T, B> b) {
  return {[a = std::move(a), b = std::move(b)](T tokens) -> ParseResult<T, B> {
    auto result = a.run(tokens);
    if (not result) {
      return std::move(result.error());
    }
    return b.run(result.remaining());
  }};
}

template <TokenView T, typename A, typename B> Parsy<T, A> operator<(Parsy<T, A> a, Parsy<T, B> b) {
  return {[a = std::move(a), b = std::move(b)](T tokens) -> ParseResult<T, A> {
    auto result_a = a.run(tokens);
    if (not result_a) {
      return std::move(result_a.error());
    }

    auto result_b = b.run(result_a.remaining());
    if (not result_b) {
      return std::move(result_b.error());
    }
    return {std::move(result_a.value()), result_b.remaining()};
  }};
}

template <TokenView T, typename... As, std::invocable<As...> Fn>
auto seq(Fn f, Parsy<T, As>... parsers) -> Parsy<T, decltype(f(std::declval<As>()...))> {
  return {[f = std::move(f), ... parsers = std::move(parsers)](
              T tokens) -> ParseResult<T, decltype(f(std::declval<As>()...))> {
    auto results = detail::seq_impl(tokens, parsers...);
    if (not results) {
      return std::move(results.error());
    }

    return std::apply(
        [&](auto &&...results_unwrapped) -> ParseResult<T, decltype(f(std::declval<As>()...))> {
          return {
              f(std::move(results_unwrapped)...),
              results.remaining(),
          };
        },
        std::move(results.value()) //
    );
  }};
}

template <TokenView T, typename A, std::size_t N>
Parsy<T, A> any(std::array<Parsy<T, A>, N> parsers) {
  return {[parsers = std::move(parsers)](T tokens) -> ParseResult<T, A> {
    std::vector<ParseError<T>> errors;
    for (auto &parser : parsers) {
      auto result = parser.run(tokens);
      if (result) {
        return result;
      }
      if (not result.error().is_recoverable) {
        return std::move(result.error());
      }
      errors.push_back(std::move(result.error()));
    }

    return ParseError<T>{
        .error = errors::Combined<T>{.pos = tokens.pos(), .errors = std::move(errors)},
        .is_recoverable = true,
    };
  }};
}

template <TokenView T, typename A> Parsy<T, std::vector<A>> many(Parsy<T, A> parser) {
  return {[parser = std::move(parser)](T tokens) -> ParseResult<T, std::vector<A>> {
    std::vector<A> out;
    while (not tokens.empty()) {
      auto result = parser.run(tokens);
      if (not result) {
        if (not result.error().is_recoverable) {
          return std::move(result.error());
        }
        break;
      }
      out.push_back(std::move(result.value()));
      tokens = result.remaining();
    }
    return {std::move(out), tokens};
  }};
}

template <TokenView T, typename A> Parsy<T, std::vector<A>> some(Parsy<T, A> parser) {
  return {[parser = std::move(parser)](T tokens) -> ParseResult<T, std::vector<A>> {
    std::vector<A> out;
    while (not tokens.empty()) {
      auto result = parser.run(tokens);
      if (not result) {
        if (not result.error().is_recoverable) {
          return std::move(result.error());
        }
        break;
      }
      out.push_back(std::move(result.value()));
      tokens = result.remaining();
    }
    if (out.empty()) {
      return ParseError<T>{
          .error =
              errors::Unexpected<T>{
                  .pos = tokens.pos(),
                  .expected = "at least one",
              },
          .is_recoverable = true,
      };
    }

    return {std::move(out), tokens};
  }};
}

template <TokenView T, typename A> Parsy<T, std::optional<A>> optional(Parsy<T, A> parser) {
  return {[parser = std::move(parser)](T tokens) -> ParseResult<T, std::optional<A>> {
    auto result = parser.run(tokens);
    if (not result) {
      if (result.error().is_recoverable) {
        return {std::nullopt, tokens};
      }
      return std::move(result.error());
    }
    return {std::move(result.value()), result.remaining()};
  }};
}

template <TokenView T, std::predicate<typename TokenTraits<T>::value> Pred>
Parsy<T, T> satisfies(MeaningfulPredicate<Pred> pred) {
  return {[pred = std::move(pred)](T tokens) -> ParseResult<T, T> {
    if (tokens.empty() or not pred.fn(tokens.head())) {
      return ParseError<T>{
          .error = errors::Unexpected<T>{.pos = tokens.pos(), .expected = pred.meaning},
          .is_recoverable = true,
      };
    }

    return {tokens.take(1), tokens.next()};
  }};
}

template <TokenView T, std::predicate<typename TokenTraits<T>::value> Pred>
Parsy<T, T> many_where(Pred pred) {
  return {[pred = std::move(pred)](T tokens) -> ParseResult<T, T> {
    std::size_t matched_cnt = 0;
    auto remaining_tokens = tokens;
    while (not remaining_tokens.empty() and pred(remaining_tokens.head())) {
      remaining_tokens = remaining_tokens.next();
      ++matched_cnt;
    }

    return {tokens.take(matched_cnt), remaining_tokens};
  }};
}

template <TokenView T, std::predicate<typename TokenTraits<T>::value> Pred>
Parsy<T, T> some_where(MeaningfulPredicate<Pred> pred) {
  return {[pred = std::move(pred)](T tokens) -> ParseResult<T, T> {
    std::size_t matched_cnt = 0;
    auto remaining_tokens = tokens;
    while (not remaining_tokens.empty() and pred.fn(remaining_tokens.head())) {
      remaining_tokens = remaining_tokens.next();
      ++matched_cnt;
    }

    if (matched_cnt == 0) {
      return ParseError<T>{
          .error = errors::Unexpected<T>{.pos = tokens.pos(), .expected = pred.meaning},
          .is_recoverable = true,
      };
    }

    return {tokens.take(matched_cnt), remaining_tokens};
  }};
}

} // namespace parsy

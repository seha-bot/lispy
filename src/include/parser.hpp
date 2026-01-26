#ifndef PARSER_HPP
#define PARSER_HPP

#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace parse {

template <typename T>
auto pure(T value);

struct Pos {
    int line, col;
};
struct Err {
    std::string what;
    Pos where;
};

template <typename T>
struct Parser {
    struct Ok {
        T value;
        std::string_view rest;
        Pos where;
    };
    using Result = std::expected<Ok, Err>;

    std::function<Result(std::string_view, Pos)> run;

    auto map(auto f) {
        return *this + [f = std::move(f)](T x) { return pure(f(x)); };
    }

    template <typename U = T>
    Parser<U> operator>(Parser<U> p) const {
        return *this + [p](T const&) { return p; };
    }

    template <typename U = T>
    Parser operator<(Parser<U> p) const {
        return *this + [p](T value) { return p > pure(std::move(value)); };
    }

    auto operator+(auto&& f) const {
        using P = std::remove_reference_t<decltype(f(std::declval<T>()))>;
        using R = typename P::Result;
        return P{[run = run, f = std::move(f)](std::string_view s, Pos p) -> R {
            if (auto r = run(s, p)) {
                return f(r->value).run(r->rest, r->where);
            } else {
                return std::unexpected(r.error());
            }
        }};
    }

    static Parser fail(std::string what) {
        return Parser{[what = std::move(what)](std::string_view, Pos p) { return std::unexpected(Err{what, p}); }};
    }

    Parser operator||(Parser alt) const {
        return Parser{[run = run, alt = std::move(alt)](std::string_view s, Pos p) {
            auto r = run(s, p);
            if (!r) {
                return alt.run(s, p);
            }
            return r;
        }};
    }

    Parser<std::vector<T>> some() {
        using R = typename Parser<std::vector<T>>::Result;
        return Parser<std::vector<T>>{[run = std::move(run)](std::string_view s, Pos p) -> R {
            std::vector<T> v;
            while (true) {
                auto r = run(s, p);
                if (!r) {
                    if (v.empty()) {
                        return std::unexpected(r.error());
                    } else {
                        break;
                    }
                }
                v.push_back(r->value);
                s = r->rest;
                p = r->where;
            }
            return R({v, s, p});
        }};
    }

    Parser<std::vector<T>> many() {
        using R = typename Parser<std::vector<T>>::Result;
        return Parser<std::vector<T>>{[run = std::move(run)](std::string_view s, Pos p) -> R {
            std::vector<T> v;
            while (true) {
                auto r = run(s, p);
                if (!r) {
                    break;
                }
                v.push_back(r->value);
                s = r->rest;
                p = r->where;
            }
            return R({v, s, p});
        }};
    }
};

template <typename T>
auto pure(T value) {
    using P = Parser<T>;
    using R = typename P::Result;
    using Ok = typename P::Ok;
    return P{[value = value](std::string_view s, Pos pos) -> R { return Ok{value, s, pos}; }};
}

template <typename T>
Parser<std::tuple<T>> sequence(Parser<T> x) {
    return x.map([](T y) { return std::make_tuple(y); });
}

template <typename T, typename... Ts>
Parser<std::tuple<T, Ts...>> sequence(Parser<T> x, Parser<Ts>... xs) {
    return x + [=](T y) {
        return sequence(xs...).map([=](std::tuple<Ts...> ys) { return std::tuple_cat(std::make_tuple(y), ys); });
    };
}

Parser<char> satisfy(std::function<bool(char)> f) {
    using P = Parser<char>;
    return P{[f](std::string_view s, Pos pos) -> P::Result {
        if (not s.empty() and f(s[0])) {
            ++pos.col;
            if (s[0] == '\n') {
                ++pos.line;
                pos.col = 1;
            }
            return P::Ok{s[0], s.substr(1), pos};
        }
        return std::unexpected(Err{"unsatisified predicate", pos});
    }};
}

Parser<char> char_(char c) {
    return satisfy([c](char x) { return x == c; }) or Parser<char>::fail(std::string("expected '") + c + "'");
}

Parser<int> ws() {
    return satisfy([](char c) { return std::isspace(c); }).many() > pure(0);
}

}  // namespace parse

#endif

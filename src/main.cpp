#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

extern "C" {
#include <readline/history.h>
#include <readline/readline.h>
}

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

struct Expr {
    using Env = std::unordered_map<std::string_view, Expr *>;
    virtual ~Expr() = default;
    virtual std::string format(bool = true) const = 0;
    virtual Expr *eval(Env& env) const = 0;
};

struct ExprAtom : Expr {
    std::string value;
    ExprAtom(std::string value) : value(std::move(value)) {}

    std::string format(bool) const override { return value; }
    Expr *eval(Env& env) const override { return env.at(value); }
};
struct ExprCons : Expr {
    Expr *car, *cdr;
    ExprCons(Expr *car, Expr *cdr) : car(car), cdr(cdr) {}

    std::string format(bool parens = true) const override {
        std::string r;
        if (parens) {
            r += '(';
        }
        r += car->format(true);
        if (auto *e = dynamic_cast<ExprAtom *>(cdr); not e or e->value != "NIL") {
            r += ' ' + cdr->format(false);
        }
        if (parens) {
            r += ')';
        }
        return r;
    }
    Expr *eval(Env& env) const override;
};

template <>
struct std::hash<ExprAtom> {
    std::size_t operator()(const ExprAtom& e) const noexcept { return std::hash<std::string_view>()(e.value); }
};

template <>
struct std::hash<ExprCons> {
    std::size_t operator()(const ExprCons& e) const noexcept {
        return std::hash<Expr *>()(e.car) ^ std::hash<Expr *>()(e.cdr);
    }
};

Expr *car_(Expr *e) { return dynamic_cast<ExprCons&>(*e).car; }
Expr *cdr_(Expr *e) { return dynamic_cast<ExprCons&>(*e).cdr; }
bool is_atom(Expr *e) { return dynamic_cast<ExprAtom *>(e) != nullptr; }

struct Memory {
    std::vector<ExprAtom *> atom_storage{new ExprAtom("NIL"), new ExprAtom("t"), new ExprAtom("f")};
    std::vector<ExprCons *> cons_storage;
    ExprAtom *alloc_atom(std::string value) {
        for (auto x : atom_storage) {
            if (x->value == value) {
                return x;
            }
        }
        return atom_storage.emplace_back(new ExprAtom(std::move(value)));
    }
    ExprCons *alloc_cons(Expr *a, Expr *b) { return cons_storage.emplace_back(new ExprCons(a, b)); }
    ExprAtom *nil() { return atom_storage[0]; }
    ExprAtom *true_() { return atom_storage[1]; }
    ExprAtom *false_() { return atom_storage[2]; }
    ExprAtom *check(bool b) { return b ? true_() : false_(); }

    // void mark_for_destruction(Expr *expr);
    // void sweep();

    ~Memory() {
        for (auto x : atom_storage) delete x;
        for (auto x : cons_storage) delete x;
    }
};

Memory mem;

Expr *ExprCons::eval(Env& env) const {
    if (auto *a = dynamic_cast<ExprAtom *>(car)) {
        if (a->value == "QUOTE") {
            return car_(cdr);
        } else if (a->value == "ATOM") {
            auto expr = car_(cdr)->eval(env);
            return mem.check(is_atom(expr));
        } else if (a->value == "EQ") {
            auto x = car_(cdr)->eval(env);
            auto y = car_(cdr_(cdr))->eval(env);
            return mem.check(x == y);
        } else if (a->value == "COND") {
            auto branches = cdr;
            while (true) {
                auto p = car_(car_(branches))->eval(env);
                if (p == mem.true_()) {
                    return car_(cdr_(car_(branches)))->eval(env);
                } else {
                    branches = cdr_(branches);
                }
            }
        } else if (a->value == "CAR") {
            return car_(car_(cdr)->eval(env));
        } else if (a->value == "CDR") {
            return cdr_(car_(cdr)->eval(env));
        } else if (a->value == "CONS") {
            auto x = car_(cdr)->eval(env);
            auto xs = car_(cdr_(cdr))->eval(env);
            return mem.alloc_cons(x, xs);
        } else {
            auto v = env.at(a->value);
            auto evlis = [&env](Expr *e) {
                std::vector<Expr *> vec;
                while (e != mem.nil()) {
                    vec.push_back(car_(e)->eval(env));
                    e = cdr_(e);
                }
                Expr *ret = mem.nil();
                for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                    ret = mem.alloc_cons(*it, ret);
                }
                return ret;
            };
            return mem.alloc_cons(v, evlis(cdr))->eval(env);
        }
    }
    throw std::runtime_error("unimplemented for " + format());
}

template <typename T>
struct Parser;

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
    using Result = std::variant<Ok, Err>;

    std::function<Result(std::string_view, Pos)> run;

    auto map(auto&& f) {
        return *this + [f](T x) { return pure(f(x)); };
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
        return P{[run = run, f = FWD(f)](std::string_view s, Pos p) -> R {
            return std::visit(
                overload{[&f](Ok r) { return R(f(r.value).run(r.rest, r.where)); }, [](Err e) { return R(e); }},
                run(s, p));
        }};
    }

    static Parser fail(std::string what) {
        return Parser{[what = std::move(what)](std::string_view, Pos p) { return Err{what, p}; }};
    }

    Parser operator||(Parser alt) const {
        return Parser{[run = run, alt = std::move(alt)](std::string_view s, Pos p) {
            auto r = run(s, p);
            if (std::holds_alternative<Err>(r)) {
                return alt.run(s, p);
            }
            return r;
        }};
    }

    Parser<std::vector<T>> some() {
        using R = typename Parser<std::vector<T>>::Result;
        using ROk = typename Parser<std::vector<T>>::Ok;
        return Parser<std::vector<T>>{[run = std::move(run)](std::string_view s, Pos p) -> R {
            std::vector<T> v;
            while (true) {
                auto r = run(s, p);
                if (auto *e = std::get_if<Err>(&r)) {
                    if (v.empty()) {
                        return *e;
                    } else {
                        break;
                    }
                }
                auto ok = std::get<Ok>(r);
                v.push_back(ok.value);
                s = ok.rest;
                p = ok.where;
            }
            return ROk{v, s, p};
        }};
    }

    Parser<std::vector<T>> many() {
        using R = typename Parser<std::vector<T>>::Result;
        using ROk = typename Parser<std::vector<T>>::Ok;
        return Parser<std::vector<T>>{[run = std::move(run)](std::string_view s, Pos p) -> R {
            std::vector<T> v;
            while (true) {
                auto r = run(s, p);
                if (auto *e = std::get_if<Err>(&r)) {
                    break;
                }
                auto ok = std::get<Ok>(r);
                v.push_back(ok.value);
                s = ok.rest;
                p = ok.where;
            }
            return ROk{v, s, p};
        }};
    }
};

template struct Parser<char>;

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
        return Err{"unsatisified predicate", pos};
    }};
}

Parser<char> char_(char c) {
    return satisfy([c](char x) { return x == c; }) or Parser<char>::fail(std::string("expected '") + c + "'");
}

Parser<int> ws() {
    return satisfy([](char c) { return std::isspace(c); }).many().map([](auto&&) { return 0; });
}

Parser<char> atom_part() {
    return satisfy([](char c) { return not std::isspace(c) and c != '(' and c != ')' and c != '\''; });
}

Parser<Expr *> atom() {
    return atom_part().some().map(
        [](std::vector<char> const& v) -> Expr * { return mem.alloc_atom(std::string(v.begin(), v.end())); });
}

template <typename T>
Parser<T> recurse(Parser<T> (*f)()) {
    return pure(0) + [f](int) { return f(); };
}

Parser<std::optional<char>> maybe_quote() {
    return char_('\'').map([](char c) { return std::make_optional(c); }) or pure(std::optional<char>());
}

Parser<Expr *> s_expr() {
    return ws() >
           sequence(maybe_quote(), atom() or char_('(') > recurse(s_expr).many().map([](std::vector<Expr *> const& v) {
               Expr *acc = mem.nil();
               for (auto it = v.rbegin(); it != v.rend(); ++it) {
                   acc = mem.alloc_cons(*it, acc);
               }
               return acc;
           }) < char_(')'))
               .map([](std::tuple<std::optional<char>, Expr *> x) -> Expr * {
                   if (std::get<0>(x)) {
                       return mem.alloc_cons(mem.alloc_atom("QUOTE"), mem.alloc_cons(std::get<1>(x), mem.nil()));
                   } else {
                       return std::get<1>(x);
                   }
               });
}

struct FreeDeleter {
    static void operator()(void *ptr) { std::free(ptr); }
};

int main() {
    auto p = s_expr();
    Expr::Env e;

    using_history();

    while (auto input = std::unique_ptr<char, FreeDeleter>(readline("> "))) {
        add_history(input.get());

        std::visit(overload{
                       [&](Parser<Expr *>::Ok r) {
                           if (not r.rest.empty()) {
                               std::cout << "[Warning] trailing_characters: \"" << r.rest << "\"\n";
                           }
                           std::cout << "[Debug] prog: " << r.value->format() << '\n';
                           try {
                               std::cout << r.value->eval(e)->format() << '\n';
                           } catch (std::exception const& err) {
                               std::cout << "Runtime error: " << err.what() << '\n';
                           }
                       },
                       [](Err e) {
                           std::cout << "Error at " << e.where.line << ", " << e.where.col << ": " << e.what << '\n';
                       },
                   },
                   p.run(input.get(), Pos{1, 1}));
    }
}

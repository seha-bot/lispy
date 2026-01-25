#include <cctype>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

// 1. atom
// 2. eq
// 3. car
// 4. cdr
// 5. cons
// 6. cond
// 7. quote

struct Expr {
    // std::unique_ptr<Expr> lol;
    virtual ~Expr() = default;

    virtual void print() const = 0;
};

struct ExprAtom : Expr {
    ExprAtom(std::string value) : value(std::move(value)) {}
    std::string value;

    virtual void print() const {
        std::cout << value;
    }
};
struct ExprEq : Expr {};
struct ExprCar : Expr {};
struct ExprCdr : Expr {};
struct ExprCons : Expr {
    ExprCons(std::shared_ptr<Expr> car, std::shared_ptr<Expr> cdr) : car(car), cdr(cdr) {}
    std::shared_ptr<Expr> car, cdr;

    virtual void print() const {
        if (dynamic_cast<ExprAtom*>(car.get())) {
            car->print();
        } else {
            std::cout << '(';
            car->print();
            std::cout << ')';
        }
        std::cout << ' ';
        cdr->print();
    }
};
struct ExprCond : Expr {};
struct ExprQuote : Expr {};

std::shared_ptr<Expr> nil() { return std::make_shared<ExprAtom>("NIL"); }

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

    // Parser empty() const {
    //     return Parser{[](std::string_view) { return std::nullopt; }};
    // }

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
        return Err{"", pos};
    }};
}

Parser<char> char_(char c) {
    return satisfy([c](char x) { return x == c; });
}

Parser<int> ws() {
    return satisfy([](char c) { return std::isspace(c); }).many().map([](auto&&) { return 0; });
}

Parser<char> atom_part() {
    return satisfy([](char c) { return not std::isspace(c) and c != '(' and c != ')'; });
}

Parser<ExprAtom> atom() {
    return atom_part().some().map([](std::vector<char> const& v) { return ExprAtom{std::string(v.begin(), v.end())}; });
}

template <typename T>
Parser<T> recurse(Parser<T> (*f)()) {
    return pure(0) + [f](int) { return f(); };
}

auto atom_to_expr = [](ExprAtom x) -> std::shared_ptr<Expr> { return std::make_shared<ExprAtom>(x); };

Parser<std::shared_ptr<Expr>> s_expr() {
    return ws() > (atom().map(atom_to_expr) or char_('(') > recurse(s_expr) < char_(')'))
                      .many()
                      .map([](std::vector<std::shared_ptr<Expr>> const& v) {
                          std::shared_ptr<Expr> acc = nil();
                          for (int i = static_cast<int>(v.size()) - 1; i >= 0; --i) {
                              acc = std::make_shared<ExprCons>(v[i], acc);
                          }
                          return acc;
                      });
}

int main() {
    auto p = s_expr();
    std::visit(
        overload{
            [](Parser<std::shared_ptr<Expr>>::Ok r) {
                r.value->print();
                std::cout << '\n';
            },
            [](Err e) { std::cout << "Error at " << e.where.line << ", " << e.where.col << ": " << e.what << '\n'; },
        },
        p.run("a", Pos{1, 1}));
        // p.run("((a)b)", Pos{1, 1}));
    // p.run("hi(hello)sup((wow)zoo)", Pos{1, 1}));
    // p.run("hi(hello)hi(sup(zoo))", Pos{1, 1}));
}

#ifndef AST_HPP
#define AST_HPP

#include <cstdint>
#include <memory>
#include <ranges>
#include <string>
#include <utility>

namespace ast {

struct Expr {
    virtual ~Expr() = default;
    virtual std::string format() const = 0;
};

struct Atom : Expr {
    Atom(std::string value) : value(std::move(value)) {}

    std::string format() const override { return value; }

    std::string value;
};

struct NumberLiteral : Expr {
    NumberLiteral(std::int64_t value) : value(value) {}

    std::string format() const override { return std::to_string(value); }

    std::int64_t value;
};

struct StringLiteral : Expr {
    StringLiteral(std::string value) : value(std::move(value)) {}

    std::string format() const override { return "\"" + value + "\""; }
    std::string value;
};

struct List : Expr {
    List(std::vector<std::unique_ptr<Expr>> list) : list(std::move(list)) {}

    std::string format() const override {
        std::string r = "(";
        r.append_range(list | std::views::transform([](auto const& x) { return x->format(); }) |
                       std::views::join_with(' '));
        r.push_back(')');
        return r;
    }

    std::vector<std::unique_ptr<Expr>> list;
};

}  // namespace ast

#endif

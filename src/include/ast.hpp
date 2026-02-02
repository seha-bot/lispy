#ifndef AST_HPP
#define AST_HPP

#include <cstdint>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

namespace ast {

enum class ExprType { atom, list, number, string };

struct Expr {
    virtual ~Expr() = default;
    virtual std::string format() const = 0;
    virtual ExprType type() const = 0;

    bool is_atom() const { return type() == ExprType::atom; }
    bool is_list() const { return type() == ExprType::list; }
    bool is_number() const { return type() == ExprType::number; }
    bool is_string() const { return type() == ExprType::string; }
};

struct Atom : Expr {
    Atom(std::string value) : value(std::move(value)) {}

    std::string format() const override { return value; }
    ExprType type() const override { return ExprType::atom; }

    std::string value;
};

struct List : Expr {
    List(std::vector<std::unique_ptr<Expr>> list) : elements(std::move(list)) {}

    std::string format() const override {
        std::string r = "(";
        r.append_range(elements                                                            //
                       | std::views::transform([](auto const& x) { return x->format(); })  //
                       | std::views::join_with(' '));
        r.push_back(')');
        return r;
    }

    ExprType type() const override { return ExprType::list; }

    std::vector<std::unique_ptr<Expr>> elements;
};

struct Number : Expr {
    Number(std::int64_t value) : value(value) {}

    std::string format() const override { return std::to_string(value); }
    ExprType type() const override { return ExprType::number; }

    std::int64_t value;
};

struct String : Expr {
    String(std::string value) : value(std::move(value)) {}

    std::string format() const override { return '"' + value + '"'; }
    ExprType type() const override { return ExprType::string; }

    std::string value;
};

}  // namespace ast

#endif

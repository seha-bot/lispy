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

struct Source {
    std::size_t position;
    int line;
    int col;
};

struct Expr {
    Expr(Source source) : m_source(source) {}

    Expr(Expr const&) = delete;
    Expr& operator=(Expr const&) = delete;

    virtual ~Expr() = default;
    virtual std::string format() const = 0;
    virtual ExprType type() const = 0;

    Source source() const { return m_source; }

    bool is_atom() const { return type() == ExprType::atom; }
    bool is_list() const { return type() == ExprType::list; }
    bool is_number() const { return type() == ExprType::number; }
    bool is_string() const { return type() == ExprType::string; }

private:
    Source m_source;
};

using ExprPtr = std::unique_ptr<Expr>;

struct Atom : Expr {
    Atom(std::string value, Source source) : Expr(source), m_name(std::move(value)) {}

    std::string format() const override { return m_name; }
    ExprType type() const override { return ExprType::atom; }
    std::string const& name() const { return m_name; }

private:
    std::string m_name;
};

struct List : Expr {
    List(std::vector<ExprPtr> list, Source source) : Expr(source), m_elements(std::move(list)) {}

    std::string format() const override {
        std::string r = "(";
        r.append_range(m_elements                                                          //
                       | std::views::transform([](auto const& x) { return x->format(); })  //
                       | std::views::join_with(' '));
        r.push_back(')');
        return r;
    }

    ExprType type() const override { return ExprType::list; }
    bool empty() const { return m_elements.empty(); }
    std::size_t size() const { return m_elements.size(); }
    std::vector<ExprPtr> const& elements() const { return m_elements; }
    Expr& operator[](std::size_t i) const { return *m_elements[i]; }

private:
    std::vector<ExprPtr> m_elements;
};

struct Number : Expr {
    Number(std::int64_t value, Source source) : Expr(source), m_value(value) {}

    std::string format() const override { return std::to_string(m_value); }
    ExprType type() const override { return ExprType::number; }
    std::int64_t value() const { return m_value; }

private:
    std::int64_t m_value;
};

struct String : Expr {
    String(std::string value, Source source) : Expr(source), m_value(std::move(value)) {}

    std::string format() const override { return '"' + m_value + '"'; }
    ExprType type() const override { return ExprType::string; }

private:
    std::string m_value;
};

}  // namespace ast

#endif

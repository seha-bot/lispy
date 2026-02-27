#ifndef AST_HPP
#define AST_HPP

#include <cstdint>
#include <memory>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

// structured AST
namespace sast {

struct Expr {
    virtual ~Expr() = default;
};

using ExprPtr = std::unique_ptr<Expr>;

struct Call : Expr {
    Call(ExprPtr callee, std::vector<ExprPtr> arguments)
        : m_callee(std::move(callee)), m_arguments(std::move(arguments)) {}

    ExprPtr m_callee;
    std::vector<ExprPtr> m_arguments;
};

struct Pattern : Expr {
    Pattern(std::string constructor_name, std::vector<std::string> values)
        : m_constructor_name(std::move(constructor_name)), m_values(std::move(values)) {}

    std::string m_constructor_name;
    std::vector<std::string> m_values;
};

struct Case : Expr {
    Case(ExprPtr expr, std::vector<std::pair<Pattern, ExprPtr>> cases)
        : m_expr(std::move(expr)), m_cases(std::move(cases)) {}

    ExprPtr m_expr;
    std::vector<std::pair<Pattern, ExprPtr>> m_cases;
};

struct ValueDefinition {};

struct Type {
    Type(std::string name, std::vector<std::unique_ptr<Type>> arguments)
        : m_name(std::move(name)), m_arguments(std::move(arguments)) {}

    std::string m_name;
    std::vector<std::unique_ptr<Type>> m_arguments;
};

struct FunctionDefinition {
    FunctionDefinition(Type type, std::string name, std::vector<std::string> args, ExprPtr body)
        : m_type(std::move(type)), m_name(std::move(name)), m_args(std::move(args)), m_body(std::move(body)) {}

    Type m_type;
    std::string m_name;
    std::vector<std::string> m_args;
    ExprPtr m_body;
};

struct Constructor {
    Constructor(std::string name, std::vector<Type> arguments)
        : m_name(std::move(name)), m_arguments(std::move(arguments)) {}

    std::string m_name;
    std::vector<Type> m_arguments;
};

struct TypeDefinition {
    TypeDefinition(std::string name, std::vector<Constructor> constructors)
        : m_name(std::move(name)), m_constructors(std::move(constructors)) {}

    std::string m_name;
    std::vector<Constructor> m_constructors;
};

using Definition = std::variant<ValueDefinition, FunctionDefinition, TypeDefinition>;

}  // namespace sast

// Raw AST
namespace rast {

enum class ExprType { atom, list, number, string };

struct Source {
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

struct Atom : Expr, sast::Expr {
    Atom(std::string value, Source source) : rast::Expr(source), m_name(std::move(value)) {}

    std::string format() const override { return m_name; }
    ExprType type() const override { return ExprType::atom; }
    std::string const& name() const { return m_name; }
    std::string& name() { return m_name; }

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
    std::vector<ExprPtr>& elements() { return m_elements; }
    Expr& operator[](std::size_t i) const { return *m_elements[i]; }

private:
    std::vector<ExprPtr> m_elements;
};

struct Number : Expr, sast::Expr {
    Number(std::int64_t value, Source source) : rast::Expr(source), m_value(value) {}

    std::string format() const override { return std::to_string(m_value); }
    ExprType type() const override { return ExprType::number; }
    std::int64_t value() const { return m_value; }

private:
    std::int64_t m_value;
};

struct String : Expr, sast::Expr {
    String(std::string value, Source source) : rast::Expr(source), m_value(std::move(value)) {}

    std::string format() const override { return '"' + m_value + '"'; }
    ExprType type() const override { return ExprType::string; }

private:
    std::string m_value;
};

}  // namespace rast

#endif

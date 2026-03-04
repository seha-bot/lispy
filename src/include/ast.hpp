#ifndef AST_HPP
#define AST_HPP

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace ast {

struct Type {
    Type(std::string name, std::vector<std::unique_ptr<Type>> arguments)
        : m_name(std::move(name)), m_arguments(std::move(arguments)) {}

    std::string m_name;
    std::vector<std::unique_ptr<Type>> m_arguments;
};

struct Source {
    int line;
    int col;
};

struct Atom {
    Atom(std::string value, Source) : m_name(std::move(value)) {}

    std::string const& name() const { return m_name; }
    std::string& name() { return m_name; }

private:
    std::string m_name;
};

struct Number {
    Number(std::int64_t value, Source) : m_value(value) {}

    std::int64_t value() const { return m_value; }

private:
    std::int64_t m_value;
};

struct EntityId {
    std::size_t id;
};

struct EntityReference {
    EntityId id;
};

struct List;
struct Call;
struct Case;
struct Lambda;

using Expr = std::variant<Atom, List, Number, Call, Case, Lambda, EntityReference>;
using ExprPtr = std::unique_ptr<Expr>;

struct List {
    List(std::vector<Expr> list, Source);
    bool empty() const;
    std::size_t size() const;
    Expr& operator[](std::size_t i);
    List drop(std::size_t n) &&;

private:
    std::vector<Expr> m_elements;
};

struct Call {
    ExprPtr callee;
    std::vector<Expr> arguments;
};

struct Pattern {
    std::string constructor_name;
    std::vector<std::string> values;
};

struct Case {
    ExprPtr scrutinee;
    std::vector<std::pair<Pattern, Expr>> cases;
};

struct Lambda {
    std::optional<Type> type_signature;
    EntityId parameter;
    ExprPtr body;
};

struct ShallowValueDefinition {
    std::optional<Type> type_signature;
    std::string name;
    Expr raw_value;
};

struct ShallowMacroDefinition {
    std::string name;
    Expr rest;
};

struct ShallowModule {
    std::string name;
    Expr rest;
};

struct Constructor {
    std::string name;
    std::vector<Type> arguments;
};

struct ValueDefinition {
    std::optional<Type> type_signature;
    std::string name;
    Expr value;
};

struct TypeDefinition {
    std::string name;
    std::vector<Constructor> constructors;
};

struct MacroDefinition {
    std::string name;
    std::vector<std::string> parameters;
    Expr body;
};

struct Module {
    std::string name;
    std::vector<EntityId> entities;
};

struct LambdaParameter {
    std::string name;
};

// Shallow entites are partially-compiled entities.
using ShallowEntity = std::variant<ShallowValueDefinition, TypeDefinition, ShallowMacroDefinition, ShallowModule>;

// Entities have names.
using Entity = std::variant<ValueDefinition, TypeDefinition, MacroDefinition, Module, LambdaParameter>;

inline List::List(std::vector<Expr> list, Source) : m_elements(std::move(list)) {}
inline bool List::empty() const { return m_elements.empty(); }
inline std::size_t List::size() const { return m_elements.size(); }
inline Expr& List::operator[](std::size_t i) { return m_elements[i]; }
inline List List::drop(std::size_t n) && {
    m_elements.erase(m_elements.begin(), m_elements.begin() + static_cast<std::ptrdiff_t>(n));
    return std::move(*this);
}

}  // namespace ast

#endif

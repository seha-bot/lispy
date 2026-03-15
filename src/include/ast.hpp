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

struct Source {
    int line;
    int col;
};

struct EntityId {
    std::size_t id;
};

struct Kind {
    std::optional<std::pair<std::unique_ptr<Kind>, std::unique_ptr<Kind>>> arrow;
};

struct TypeReference {
    EntityId id;
};

struct TypeLambda;
struct TypeArrow;
struct TypeVariant;
struct TypeTuple;
struct TypeApplication;

using Type = std::variant<TypeLambda, TypeArrow, TypeVariant, TypeTuple, TypeApplication, TypeReference>;

struct TypeLambda {
    std::optional<Kind> kind_signature;
    EntityId parameter;
    std::unique_ptr<Type> type;
};

struct TypeArrow {
    std::unique_ptr<Type> from, to;
};

struct TypeVariant {
    std::vector<std::pair<EntityId, std::optional<Type>>> elements;
};

struct TypeTuple {
    std::vector<Type> elements;
};

struct TypeApplication {
    std::unique_ptr<Type> function;
    std::vector<Type> arguments;
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

struct EntityReference {
    EntityId id;
};

struct List;
struct Call;
struct Case;
struct Lambda;
struct Quantifier;

using Expr = std::variant<Atom, List, Number, Call, Case, Lambda, Quantifier, EntityReference>;
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
    EntityId name;
    std::vector<EntityId> bindings;
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

struct Quantifier {
    std::optional<Kind> kind_signature;
    EntityId parameter;
    ExprPtr body;
};

struct ShallowValueDefinition {
    std::optional<Expr> raw_type_signature;
    std::string name;
    Expr raw_value;
};

struct ShallowTypeFormDefinition {
    std::string name;
    Expr raw_type;
};

struct ShallowValueDeclaration {
    std::string name;
    Expr raw_type_signature;
};

struct ShallowMacroDefinition {
    std::string name;
    Expr rest;
};

struct ShallowModuleDefinition {
    std::string name;
    List raw_entities;
};

// Shallow entites are partially-compiled entities.
using ShallowEntity = std::variant<ShallowValueDefinition, ShallowTypeFormDefinition, ShallowValueDeclaration,
                                   ShallowMacroDefinition, ShallowModuleDefinition>;

struct ValueDefinition {
    std::optional<Type> type_signature;
    std::string name;
    Expr value;
};

struct Constructor {
    std::string name;
    std::vector<Type> parameters;
};

struct TypeFormDefinition {
    std::string name;
    Type type;
};

// NOTE: don't forget that you do not want polymorphic declarations.
struct ValueDeclaration {
    std::string name;
    Type type_signature;
};

struct MacroDefinition {
    std::string name;
    std::vector<std::string> parameters;
    Expr body;
};

struct ModuleDefinition {
    std::string name;
    std::vector<EntityId> entities;
};

struct LambdaParameter {
    std::string name;
};

struct QuantifierParameter {
    std::string name;
};

struct Label {
    std::string name;
};

// Entities have names.
using Entity = std::variant<ValueDefinition, TypeFormDefinition, ValueDeclaration, MacroDefinition, ModuleDefinition,
                            LambdaParameter, QuantifierParameter, Label>;

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

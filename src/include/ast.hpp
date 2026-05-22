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

namespace compiler {
struct RepresentativeSets;
}

namespace compiler {
struct TypeStorage;
}

namespace ast {

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

struct List;

using RawExpr = std::variant<Atom, List, Number>;
using RawExprPtr = std::unique_ptr<RawExpr>;

struct List {
    List(std::vector<RawExpr> list, Source);
    bool empty() const;
    std::size_t size() const;
    RawExpr& operator[](std::size_t i);
    List drop(std::size_t n) &&;

private:
    std::vector<RawExpr> m_elements;
};

inline List::List(std::vector<RawExpr> list, Source) : m_elements(std::move(list)) {}
inline bool List::empty() const { return m_elements.empty(); }
inline std::size_t List::size() const { return m_elements.size(); }
inline RawExpr& List::operator[](std::size_t i) { return m_elements[i]; }
inline List List::drop(std::size_t n) && {
    m_elements.erase(m_elements.begin(), m_elements.begin() + static_cast<std::ptrdiff_t>(n));
    return std::move(*this);
}

struct EntityId {
    bool operator==(EntityId const&) const = default;
    std::size_t id;
};

struct Label {
    bool operator==(Label const&) const = default;
    std::string name;
};

struct LabelId {
    bool operator==(LabelId const&) const = default;
    std::size_t id;
};

struct TypeId {
    explicit TypeId(std::size_t id, compiler::RepresentativeSets& rep) : m_id(id), m_rep(&rep) {}

    bool operator==(TypeId const& that) const;
    std::string to_string() const;

private:
    friend compiler::RepresentativeSets;
    friend compiler::TypeStorage;
    std::size_t m_id;
    compiler::RepresentativeSets *m_rep;
};

struct Kind {
    bool operator==(Kind const&) const = default;
    std::optional<std::pair<std::shared_ptr<Kind>, std::shared_ptr<Kind>>> arrow;
};

// TODO: this is more of a "NamedType"
struct TypeReference {
    bool operator==(TypeReference const&) const = default;
    EntityId definition;
};

struct TTLambda {
    bool operator==(TTLambda const&) const = default;
    struct Parameter {
        std::string name;
    };

    EntityId parameter;
    std::optional<Kind> parameter_kind;
    TypeId body;
};

struct TypeArrow {
    bool operator==(TypeArrow const&) const = default;
    TypeId from;
    TypeId to;
};

struct TypeVariant {
    bool operator==(TypeVariant const&) const = default;
    std::vector<std::pair<LabelId, std::optional<TypeId>>> elements;
};

struct TypeTuple {
    bool operator==(TypeTuple const&) const = default;
    std::vector<TypeId> elements;
};

struct TypeApplication {
    bool operator==(TypeApplication const&) const = default;
    TypeId function;
    std::vector<TypeId> arguments;
};

struct TypeVariable {
    bool operator==(TypeVariable const&) const = default;
    std::size_t id;
};

using Type = std::variant<TypeReference, TTLambda, TypeArrow, TypeVariant, TypeTuple,
                          TypeApplication, TypeVariable>;

struct Call;
struct Case;
struct LabelCall;
struct Lambda;
struct TVLambda;

struct EntityReference {
    EntityId id;
};

using Expr = std::variant<Number, Call, Case, LabelCall, Lambda, TVLambda, EntityReference>;
using ExprPtr = std::unique_ptr<Expr>;

struct Call {
    ExprPtr callee;
    std::vector<Expr> arguments;
};

struct Case {
    struct Pattern {
        LabelId name;
        std::optional<EntityId> variable;
    };

    struct Alternative;

    ExprPtr scrutinee;
    std::vector<Alternative> alternatives;
};

struct LabelCall {
    // TODO: feel free to rename it now to label_id
    LabelId callee;
    TypeId type;
    std::optional<ExprPtr> argument;
};

struct Lambda {
    struct Parameter {
        std::string name;
        std::optional<TypeId> type;
    };

    EntityId parameter;
    ExprPtr body;
};

struct TVLambda {
    struct Parameter {
        std::string name;
    };

    EntityId parameter;
    std::optional<Kind> parameter_kind;
    ExprPtr body;
};

struct Case::Alternative {
    Pattern pattern;
    Expr arm;
};

struct ShallowValueDefinition {
    std::string name;
    RawExpr raw_value;
};

struct ShallowMergedValueDefinition {
    std::string name;
    RawExpr raw_type_signature;
    RawExpr raw_value;
};

struct ShallowTypeFormDefinition {
    std::string name;
    RawExpr raw_type;
};

struct ShallowValueDeclaration {
    std::string name;
    RawExpr raw_type_signature;
};

struct ShallowModuleDefinition {
    std::string name;
    List raw_entities;
};

// Shallow entites are partially-compiled entities.
using ShallowEntity =
    std::variant<ShallowMergedValueDefinition, ShallowValueDefinition, ShallowTypeFormDefinition,
                 ShallowValueDeclaration, ShallowModuleDefinition>;

struct ValueDefinition {
    std::string name;
    Expr value;
};

struct TypeFormDefinition {
    std::string name;
    TypeId type;
};

struct ValueDeclaration {
    std::string name;
    TypeId type_signature;
};

struct MergedValueDefinition {
    std::string name;
    TypeId type_signature;
    Expr value;
};

struct ModuleDefinition {
    std::string name;
    std::vector<EntityId> entities;
};

struct PatternBinding {
    std::string name;
};

// Entities have names.
using Entity = std::variant<ValueDefinition, TypeFormDefinition, ValueDeclaration,
                            MergedValueDefinition, ModuleDefinition, Lambda::Parameter,
                            TVLambda::Parameter, TTLambda::Parameter, PatternBinding>;

}  // namespace ast

template <>
struct std::hash<ast::EntityId> {
    static std::size_t operator()(ast::EntityId const& eid) {
        return eid.id;
    }
};

#endif

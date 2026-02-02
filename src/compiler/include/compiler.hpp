#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstdint>
#include <string>
#include <unordered_set>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "mnemonic.hpp"

struct AtomOperand {
    std::string name;
    bool is_stack = false;
};

struct LiteralOperand {
    std::int64_t value;
    bool is_stack = false;
};

struct CharOperand {
    char value;
};

struct LabelOperand {
    std::string name;
};

using Operand = std::variant<std::monostate, AtomOperand, LiteralOperand, CharOperand, LabelOperand>;

struct Instruction {
    Mnemonic mnemonic;
    Operand o1{};
    Operand o2{};
};

// TODO: split into local and global?
// Local should just have an id.
struct Label {
    std::string name;
};

using Line = std::variant<Instruction, Label>;

// TODO: model this better please.
// Entity should be a variant and for now either a definition or a lambda.
// Lambdas need to contain an id, parameters, captures and code
struct Entity {
    Entity(std::string name, bool is_lambda) : name(name), is_lambda(is_lambda) {}

    std::string name;
    std::unordered_set<std::string> parameters;
    std::unordered_set<std::string> captures;
    std::vector<Line> code;
    bool is_lambda = false;
};

enum class CompilationResult {
    ok,
    define_too_few_arguments,
    define_missing_name,
    incorrect_arity,
    unbound_atom,
    parameters_not_well_formed,
};

CompilationResult compile(std::vector<Entity>& entities, ast::Expr& expr);
std::string format_entity(Entity const& entity);

#endif

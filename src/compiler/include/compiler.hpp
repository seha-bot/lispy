#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "mnemonic.hpp"

struct AtomOperand {
    std::string name;
};

struct LiteralOperand {
    std::int64_t value;
    bool is_stack = false;
    std::string parameter_name{};
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
    std::unordered_map<std::string, std::size_t> parameters;
    std::vector<std::string> captures;
    std::vector<Line> code;
    bool is_lambda = false;

    int stack_index(std::string const& name) const {
        if (parameters.contains(name)) {
            return parameters.size() + captures.size() - 1 - parameters.at(name);
        } else if (auto it = std::ranges::find(captures, name); it != captures.end()) {
            return captures.size() - 1 - (it - captures.begin());
        }
        throw std::logic_error("idk what to do");
    }
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

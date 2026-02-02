#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstdint>
#include <string>
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

struct Label {
    std::string name;
};

using Line = std::variant<Instruction, Label>;

struct Entity {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<Line> code;
};

enum class CompilationResult {
    ok,
    define_too_few_arguments,
    define_missing_name,
    define_param_is_not_an_atom,
    calling_nonatom,
    incorrect_arity,
};

CompilationResult compile(std::vector<Entity>& entities, ast::Expr& expr);
std::string format_entity(Entity const& entity);

#endif

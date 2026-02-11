#ifndef COMPILER_HPP
#define COMPILER_HPP

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <variant>
#include <vector>

#include "explorer.hpp"
#include "mnemonic.hpp"

namespace compiler {

struct AtomOperand {
    friend std::ostream& operator<<(std::ostream& os, AtomOperand const& x) { return os << '\'' << x.name; }
    std::string name;
};

struct StackOperand {
    friend std::ostream& operator<<(std::ostream& os, StackOperand const& x) { return os << '[' << x.i << ']'; }
    std::size_t i;
};

struct LiteralOperand {
    friend std::ostream& operator<<(std::ostream& os, LiteralOperand const& x) { return os << x.value; }
    std::int64_t value;
};

struct CharOperand {
    friend std::ostream& operator<<(std::ostream& os, CharOperand const& x) { return os << '\'' << x.value << '\''; }
    char value;
};

struct LabelOperand {
    friend std::ostream& operator<<(std::ostream& os, LabelOperand const& x) { return os << x.name; }
    std::string name;
};

struct LocalLabelOperand {
    friend std::ostream& operator<<(std::ostream& os, LocalLabelOperand const& x) { return os << ".L" << x.id; }
    std::size_t id;
};

using Operand = std::variant<AtomOperand, StackOperand, LiteralOperand, CharOperand, LabelOperand, LocalLabelOperand>;

struct Instruction {
    friend std::ostream& operator<<(std::ostream& os, Instruction const& x) {
        os << '\t' << to_string(x.mnemonic);
        if (x.operand) {
            std::visit([&](auto& op) { os << ' ' << op; }, *x.operand);
        }
        return os;
    }
    Mnemonic mnemonic;
    std::optional<Operand> operand;
};

struct Label {
    friend std::ostream& operator<<(std::ostream& os, Label const& x) { return os << x.name << ':'; }
    std::string name;
};

struct LocalLabel {
    friend std::ostream& operator<<(std::ostream& os, LocalLabel const& x) { return os << ".L" << x.id << ':'; }
    std::size_t id;
};

using Line = std::variant<Instruction, Label, LocalLabel>;

struct Code {
    friend std::ostream& operator<<(std::ostream& os, Code const& x) {
        for (auto& line : x.lines) {
            std::visit([&](auto& l) { os << l << '\n'; }, line);
        }
        return os;
    }
    std::vector<Line> lines;
};

std::expected<Code, std::vector<StaticError>> compile_program(Program const& program);

}  // namespace compiler

#endif

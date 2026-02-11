#ifndef OPTIMIZER_HPP
#define OPTIMIZER_HPP

#include "compiler.hpp"

namespace compiler::optimizer {

void magic_functions(Code& code) {
    for (std::size_t i = 0; i < code.lines.size(); ++i) {
        auto *line = std::get_if<Instruction>(&code.lines[i]);
        if (not line or line->mnemonic != Mnemonic::call) {
            continue;
        }
        auto *callee = std::get_if<LabelOperand>(&*line->operand);
        if (not callee) {
            continue;
        }
        if (callee->name == "eq") {
            code.lines[i] = Instruction{Mnemonic::eq, std::nullopt};
        } else if (callee->name == "cons") {
            code.lines[i] = Instruction{Mnemonic::cons, std::nullopt};
        } else if (callee->name == "car") {
            code.lines[i] = Instruction{Mnemonic::car, std::nullopt};
        } else if (callee->name == "cdr") {
            code.lines[i] = Instruction{Mnemonic::cdr, std::nullopt};
        } else if (callee->name == "print") {
            code.lines[i] = Instruction{Mnemonic::print, std::nullopt};
        } else if (callee->name == "+") {
            code.lines[i] = Instruction{Mnemonic::iadd, std::nullopt};
        } else if (callee->name == "-") {
            code.lines[i] = Instruction{Mnemonic::ineg, std::nullopt};
        } else if (callee->name == "<") {
            code.lines[i] = Instruction{Mnemonic::iless, std::nullopt};
        } else if (callee->name == "mod") {
            code.lines[i] = Instruction{Mnemonic::imod, std::nullopt};
        }
    }
}

void push_pop(Code& code) {
    if (code.lines.empty()) {
        return;
    }

    Code res;
    for (std::size_t i = 0; i + 1 < code.lines.size(); ++i) {
        auto *push = std::get_if<Instruction>(&code.lines[i]);
        if (not push or push->mnemonic != Mnemonic::push) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        auto *pop = std::get_if<Instruction>(&code.lines[i + 1]);
        if (not pop or pop->mnemonic != Mnemonic::pop) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        ++i;
    }
    res.lines.push_back(code.lines.back());
    code.lines = std::move(res.lines);
}

void optimize(Code& code) {
    magic_functions(code);
    push_pop(code);
}

}  // namespace compiler::optimizer

#endif

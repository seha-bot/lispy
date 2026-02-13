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
            code.lines[i] = Instruction{Mnemonic::isub, std::nullopt};
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
    for (std::size_t i = 0; i < code.lines.size(); ++i) {
        if (i == code.lines.size() - 1) {
            res.lines.push_back(code.lines[i]);
            continue;
        }

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
    code.lines = std::move(res.lines);
}

void push_set(Code& code) {
    if (code.lines.empty()) {
        return;
    }

    // push [x]
    // set x + 1 | setret x + 1

    Code res;
    for (std::size_t i = 0; i < code.lines.size(); ++i) {
        if (i == code.lines.size() - 1) {
            res.lines.push_back(code.lines[i]);
            continue;
        }

        auto *push = std::get_if<Instruction>(&code.lines[i]);
        if (not push or push->mnemonic != Mnemonic::push) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        auto *push_operand = std::get_if<StackOperand>(&*push->operand);
        if (not push_operand) {
            res.lines.push_back(code.lines[i]);
            continue;
        }

        auto *set = std::get_if<Instruction>(&code.lines[i + 1]);
        if (not set or (set->mnemonic != Mnemonic::set and set->mnemonic != Mnemonic::setret)) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        auto *set_operand = std::get_if<LiteralOperand>(&*set->operand);
        if (not set_operand or push_operand->i + 1 != static_cast<std::size_t>(set_operand->value)) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        if (set->mnemonic == Mnemonic::setret) {
            res.lines.push_back(Instruction{Mnemonic::ret, LiteralOperand{set_operand->value - 1}});
        }
        ++i;
    }
    code.lines = std::move(res.lines);
}

void set1_set(Code& code) {
    if (code.lines.empty()) {
        return;
    }

    // set 1
    // set x | setret x

    Code res;
    for (std::size_t i = 0; i < code.lines.size(); ++i) {
        if (i == code.lines.size() - 1) {
            res.lines.push_back(code.lines[i]);
            continue;
        }

        auto *set1 = std::get_if<Instruction>(&code.lines[i]);
        if (not set1 or set1->mnemonic != Mnemonic::set) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        auto *set1_operand = std::get_if<LiteralOperand>(&*set1->operand);
        if (not set1_operand or set1_operand->value != 1) {
            res.lines.push_back(code.lines[i]);
            continue;
        }

        auto *set = std::get_if<Instruction>(&code.lines[i + 1]);
        if (not set or (set->mnemonic != Mnemonic::set and set->mnemonic != Mnemonic::setret)) {
            res.lines.push_back(code.lines[i]);
            continue;
        }
        auto set_operand = std::get<LiteralOperand>(*set->operand);
        res.lines.push_back(Instruction{set->mnemonic, LiteralOperand{set_operand.value + 1}});
        ++i;
    }
    code.lines = std::move(res.lines);
}

void optimize(Code& code) {
    magic_functions(code);
    push_pop(code);
    push_set(code);
    set1_set(code);
}

}  // namespace compiler::optimizer

#endif

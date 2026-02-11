#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include "bytecoder.hpp"
#include "gc.hpp"
#include "instr_ptr.hpp"
#include "mnemonic.hpp"
#include "object.hpp"

namespace {

struct Machine {
    obj::Object& unsafe_seek(std::size_t i) { return m_stack.at(m_stack.size() - 1 - static_cast<std::size_t>(i)); }
    obj::Object& unsafe_top() { return unsafe_seek(0); }

    void push(obj::Object obj) { m_stack.push_back(obj); }
    void push_call(InstrPtr ip) { m_return_stack.push_back(ip); }

    obj::Object unsafe_pop() {
        auto top = m_stack.back();
        m_stack.pop_back();
        return top;
    }
    InstrPtr unsafe_pop_call() {
        auto ip = m_return_stack.back();
        m_return_stack.pop_back();
        return ip;
    }

    bool has(std::size_t n) const { return n <= m_stack.size(); }
    std::size_t call_depth() const { return m_return_stack.size(); }

private:
    std::vector<obj::Object> m_stack;
    std::vector<InstrPtr> m_return_stack;
};

enum class StepResult {
    stack_overrun,
    call_stack_overrun,
    mismatched_type,
    corrupted_opcode,
};

std::ostream& operator<<(std::ostream& os, StepResult sr) {
    switch (sr) {
        case StepResult::stack_overrun:
            return os << "stack_overrun";
        case StepResult::call_stack_overrun:
            return os << "call_stack_overrun";
        case StepResult::mismatched_type:
            return os << "mismatched_type";
        case StepResult::corrupted_opcode:
            return os << "corrupted_opcode";
    }
}

std::expected<obj::Object, StepResult> fetch_object(Machine& m, bytecoder::Operand op) {
    switch (op.type()) {
        case bytecoder::OperandType::atom:
            return obj::Atom{static_cast<std::size_t>(op.value_unsigned())};
        case bytecoder::OperandType::literal:
            return obj::Number{op.value_signed()};
        case bytecoder::OperandType::stack: {
            auto const index = op.value_unsigned();
            if (not m.has(index + 1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            return m.unsafe_seek(static_cast<std::size_t>(index));
        }
    }

    return std::unexpected(StepResult::corrupted_opcode);
}

std::expected<std::int64_t, StepResult> fetch_literal_signed(bytecoder::Operand op) {
    if (op.type() == bytecoder::OperandType::literal) {
        return op.value_signed();
    }
    return std::unexpected(StepResult::mismatched_type);
}

std::expected<std::uint64_t, StepResult> fetch_literal_unsigned(bytecoder::Operand op) {
    if (op.type() == bytecoder::OperandType::literal) {
        return op.value_unsigned();
    }
    return std::unexpected(StepResult::mismatched_type);
}

struct Decoder {
    auto opcode(InstrPtr ip) const { return bytecoder::opcode(bc, ip); }
    auto operand(InstrPtr ip) const { return bytecoder::operand(bc, ip); }

    // TODO: this is slow unless inlined...
    std::int32_t instr_size(Mnemonic mnemonic, std::optional<bytecoder::Operand> operand = std::nullopt) {
        return static_cast<std::int32_t>(bytecoder::instr_size(bytecoder::Instruction{mnemonic, operand}));
    }

    bytecoder::Bytecode bc;
};

std::expected<InstrPtr, StepResult> step(GC& gc, obj::NameManager& mgr, Machine& m, Decoder& dec, InstrPtr ip) {
    auto opcode = dec.opcode(ip);
    switch (opcode) {
        case Mnemonic::eq: {  // eq
            if (not m.has(2)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_pop();
            m.push(obj::Number{arg1 == arg2});
            return ip.offset(dec.instr_size(Mnemonic::eq));
        }
        case Mnemonic::cons: {  // cons
            if (not m.has(2)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto rhs = m.unsafe_pop();
            auto lhs = m.unsafe_top();
            m.unsafe_top() = obj::Cons::make(gc, lhs, rhs).get();
            return ip.offset(dec.instr_size(Mnemonic::cons));
        }
        case Mnemonic::car: {  // car
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto arg1 = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not arg1 or (*arg1)->type() != obj::DynObjType::cons) {
                return std::unexpected(StepResult::mismatched_type);
            }
            m.unsafe_top() = static_cast<obj::Cons&>(**arg1).car();
            return ip.offset(dec.instr_size(Mnemonic::car));
        }
        case Mnemonic::cdr: {  // cdr
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto arg1 = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not arg1 or (*arg1)->type() != obj::DynObjType::cons) {
                return std::unexpected(StepResult::mismatched_type);
            }
            m.unsafe_top() = static_cast<obj::Cons&>(**arg1).cdr();
            return ip.offset(dec.instr_size(Mnemonic::cdr));
        }
        case Mnemonic::jt: {  // jt offset
            auto operand = dec.operand(ip);
            auto offset = fetch_literal_signed(operand);
            if (not offset) {
                return std::unexpected(offset.error());
            }

            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }

            auto cond_obj = m.unsafe_pop();
            auto cond = std::get_if<obj::Number>(&cond_obj);
            if (not cond) {
                return std::unexpected(StepResult::mismatched_type);
            }

            if ((*cond).value) {
                return ip.offset(*offset);
            }
            return ip.offset(dec.instr_size(Mnemonic::jt, operand));
        }
        case Mnemonic::jf: {  // jf offset
            auto operand = dec.operand(ip);
            auto offset = fetch_literal_signed(operand);
            if (not offset) {
                return std::unexpected(offset.error());
            }

            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }

            auto cond_obj = m.unsafe_pop();
            auto cond = std::get_if<obj::Number>(&cond_obj);
            if (not cond) {
                return std::unexpected(StepResult::mismatched_type);
            }

            if (not(*cond).value) {
                return ip.offset(*offset);
            }
            return ip.offset(dec.instr_size(Mnemonic::jf, operand));
        }
        case Mnemonic::jmp: {  // jmp offset
            auto offset = fetch_literal_signed(dec.operand(ip));
            if (not offset) {
                return std::unexpected(offset.error());
            }
            return ip.offset(*offset);
        }
        case Mnemonic::call: {  // call offset
            auto operand = dec.operand(ip);
            auto offset = fetch_literal_signed(operand);
            if (not offset) {
                return std::unexpected(offset.error());
            }
            m.push_call(ip.offset(dec.instr_size(Mnemonic::call, operand)));
            return ip.offset(*offset);
        }
        case Mnemonic::indjmp: {  // indjmp x
            auto x = fetch_object(m, dec.operand(ip));
            if (!x) {
                return std::unexpected(x.error());
            }
            struct Visitor {
                std::expected<InstrPtr, StepResult> operator()(obj::Atom) {
                    return std::unexpected(StepResult::mismatched_type);
                }
                std::expected<InstrPtr, StepResult> operator()(obj::Number dest_ip) {
                    if (dest_ip.value < 0) {
                        throw "unimplemented";
                    }
                    return InstrPtr(static_cast<std::size_t>(dest_ip.value));
                }
                std::expected<InstrPtr, StepResult> operator()(obj::DynObj *obj) {
                    if (obj->type() != obj::DynObjType::closure) {
                        return std::unexpected(StepResult::mismatched_type);
                    }
                    auto& closure = static_cast<obj::Closure&>(*obj);
                    for (auto& capture : closure.captures()) {
                        m.push(capture);
                    }
                    return closure.ip();
                }
                Machine& m;
                Decoder& dec;
                InstrPtr ip;
            };
            return std::visit(Visitor{m, dec, ip}, *x);
        }
        case Mnemonic::indcall: {  // indcall x
            auto operand = dec.operand(ip);
            auto x = fetch_object(m, operand);
            if (!x) {
                return std::unexpected(x.error());
            }
            struct Visitor {
                std::expected<InstrPtr, StepResult> operator()(obj::Atom) {
                    return std::unexpected(StepResult::mismatched_type);
                }
                std::expected<InstrPtr, StepResult> operator()(obj::Number dest_ip) {
                    if (dest_ip.value < 0) {
                        throw "unimplemented";
                    }
                    m.push_call(ip.offset(dec.instr_size(Mnemonic::indcall, operand)));
                    return InstrPtr(static_cast<std::size_t>(dest_ip.value));
                }
                std::expected<InstrPtr, StepResult> operator()(obj::DynObj *obj) {
                    if (obj->type() != obj::DynObjType::closure) {
                        return std::unexpected(StepResult::mismatched_type);
                    }
                    auto& closure = static_cast<obj::Closure&>(*obj);
                    for (auto& capture : closure.captures()) {
                        m.push(capture);
                    }
                    m.push_call(ip.offset(dec.instr_size(Mnemonic::indcall, operand)));
                    return closure.ip();
                }
                Machine& m;
                Decoder& dec;
                InstrPtr ip;
                bytecoder::Operand operand;
            };
            return std::visit(Visitor{m, dec, ip, operand}, *x);
        }
        case Mnemonic::push: {  // push x
            auto operand = dec.operand(ip);
            auto x = fetch_object(m, operand);
            if (!x) {
                return std::unexpected(x.error());
            }
            m.push(*x);
            return ip.offset(dec.instr_size(Mnemonic::push, operand));
        }
        case Mnemonic::pop: {  // pop
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            m.unsafe_pop();
            return ip.offset(dec.instr_size(Mnemonic::pop));
        }
        case Mnemonic::ret: {  // ret n
            auto n = fetch_literal_unsigned(dec.operand(ip));
            if (!n) {
                return std::unexpected(n.error());
            }
            if (not m.has(*n)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            if (m.call_depth() == 0) {
                return std::unexpected(StepResult::call_stack_overrun);
            }

            for (std::uint64_t i = 0; i < *n; ++i) {
                m.unsafe_pop();
            }
            return m.unsafe_pop_call();
        }
        case Mnemonic::set: {  // set i
            auto operand = dec.operand(ip);
            auto i = fetch_literal_unsigned(operand);
            if (not i) {
                return std::unexpected(i.error());
            }

            if (not m.has(*i + 1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            if (*i != 0) {
                auto& dest = m.unsafe_seek(static_cast<std::size_t>(*i));
                dest = m.unsafe_pop();
            }
            return ip.offset(dec.instr_size(Mnemonic::set, operand));
        }
        case Mnemonic::print: {  // print
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            obj::format_to(std::cout, mgr, m.unsafe_top());
            return ip.offset(dec.instr_size(Mnemonic::print));
        }
        case Mnemonic::mkstr:    // mkstr
        case Mnemonic::strpush:  // strpush c
            throw std::runtime_error("unimplemented");
        case Mnemonic::closure: {  // closure lam_ip
            auto operand = dec.operand(ip);
            auto lam_ip = fetch_literal_unsigned(operand);
            if (not lam_ip) {
                return std::unexpected(lam_ip.error());
            }
            m.push(obj::Closure::make(gc, InstrPtr(static_cast<std::size_t>(*lam_ip))).get());
            return ip.offset(dec.instr_size(Mnemonic::closure, operand));
        }
        case Mnemonic::capture: {  // capture x
            auto operand = dec.operand(ip);
            auto x = fetch_object(m, operand);
            if (!x) {
                return std::unexpected(x.error());
            }
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto obj = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not obj or (*obj)->type() != obj::DynObjType::closure) {
                return std::unexpected(StepResult::mismatched_type);
            }
            static_cast<obj::Closure&>(**obj).capture(*x);
            return ip.offset(dec.instr_size(Mnemonic::capture, operand));
        }
    }

    return std::unexpected(StepResult::corrupted_opcode);
}

}  // namespace

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: lispy <executable>\n";
        return EXIT_FAILURE;
    }

    InstrPtr ip(0);
    obj::NameManager mgr;
    Decoder dec;
    {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) {
            std::cerr << "Can't open file for reading.\n";
            return EXIT_FAILURE;
        }

        auto prefix = bytecoder::read_prefix(in);
        ip = prefix.entry_point;
        mgr.assign(std::move(prefix.atoms));

        while (true) {
            auto b = in.get();
            if (not in) {
                break;
            }
            dec.bc.push_back(static_cast<unsigned char>(b));
        }
    }

    GC gc;
    Machine m;
    m.push_call(InstrPtr(dec.bc.size()));
    while (true) {
        if (ip.value() >= dec.bc.size()) {
            break;
        }

        auto next_ip = step(gc, mgr, m, dec, ip);
        if (not next_ip) {
            std::cout << "[Error] at \"" << to_string(dec.opcode(ip)) << "\": " << next_ip.error() << '\n';
            return EXIT_FAILURE;
        }
        ip = *next_ip;
    }
}

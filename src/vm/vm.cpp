#include <cstddef>
#include <cstdint>
#include <expected>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "decoder.hpp"
#include "object.hpp"

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

    bool has(std::int64_t n) const { return n <= static_cast<std::int64_t>(m_stack.size()); }
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

static std::ostream& operator<<(std::ostream& os, StepResult sr) {
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

static std::expected<obj::Object, StepResult> fetch_object(Machine& m, Operand op) {
    switch (op.type) {
        case OperandType::atom:
            return obj::Atom{static_cast<std::size_t>(op.value)};
        case OperandType::literal:
            return obj::Number{op.value};
        case OperandType::stack: {
            if (op.value < 0 or not m.has(op.value + 1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            return m.unsafe_seek(static_cast<std::size_t>(op.value));
        }
    }

    return std::unexpected(StepResult::corrupted_opcode);
}

static std::expected<std::int64_t, StepResult> fetch_literal(Operand op) {
    if (op.type == OperandType::literal) {
        return op.value;
    }
    return std::unexpected(StepResult::mismatched_type);
}

static std::expected<InstrPtr, StepResult> step(GC& gc, obj::NameManager& mgr, Machine& m, Decoder& dec, InstrPtr ip) {
    switch (dec.get_opcode(ip)) {
        case Mnemonic::eq: {  // eq
            if (not m.has(2)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_pop();
            m.push(obj::Number{arg1 == arg2});
            return ip.offset(Decoder::instr_size(Mnemonic::eq));
        }
        case Mnemonic::cons: {  // cons
            if (not m.has(2)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto rhs = m.unsafe_pop();
            auto lhs = m.unsafe_top();
            m.unsafe_top() = obj::Cons::make(gc, lhs, rhs).get();
            return ip.offset(Decoder::instr_size(Mnemonic::cons));
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
            return ip.offset(Decoder::instr_size(Mnemonic::car));
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
            return ip.offset(Decoder::instr_size(Mnemonic::cdr));
        }
        case Mnemonic::jt: {  // jt offset
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }

            auto cond_obj = m.unsafe_pop();
            auto cond = std::get_if<obj::Number>(&cond_obj);
            if (not cond) {
                return std::unexpected(StepResult::mismatched_type);
            }

            if ((*cond).value) {
                return ip.offset(dec.get_offset(ip));
            }
            return ip.offset(Decoder::instr_size(Mnemonic::jt));
        }
        case Mnemonic::jf: {  // jf offset
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }

            auto cond_obj = m.unsafe_pop();
            auto cond = std::get_if<obj::Number>(&cond_obj);
            if (not cond) {
                return std::unexpected(StepResult::mismatched_type);
            }

            if (not(*cond).value) {
                return ip.offset(dec.get_offset(ip));
            }
            return ip.offset(Decoder::instr_size(Mnemonic::jf));
        }
        case Mnemonic::jmp: {  // jmp offset
            return ip.offset(dec.get_offset(ip));
        }
        case Mnemonic::call: {  // call offset
            m.push_call(ip.offset(Decoder::instr_size(Mnemonic::call)));
            return ip.offset(dec.get_offset(ip));
        }
        case Mnemonic::indjmp: {  // indjmp x
            auto x = fetch_object(m, dec.operand1(ip));
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
                InstrPtr ip;
            };
            return std::visit(Visitor{m, ip}, *x);
        }
        case Mnemonic::indcall: {  // indcall x
            auto x = fetch_object(m, dec.operand1(ip));
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
                    m.push_call(ip.offset(Decoder::instr_size(Mnemonic::indcall)));
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
                    m.push_call(ip.offset(Decoder::instr_size(Mnemonic::indcall)));
                    return closure.ip();
                }
                Machine& m;
                InstrPtr ip;
            };
            return std::visit(Visitor{m, ip}, *x);
        }
        case Mnemonic::push: {  // push x
            auto x = fetch_object(m, dec.operand1(ip));
            if (!x) {
                return std::unexpected(x.error());
            }
            m.push(*x);
            return ip.offset(Decoder::instr_size(Mnemonic::push));
        }
        case Mnemonic::pop: {  // pop n
            auto n = fetch_literal(dec.operand1(ip));
            if (!n) {
                return std::unexpected(n.error());
            }
            if (*n < 0 or not m.has(*n)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            for (std::int64_t i = 0; i < *n; ++i) {
                m.unsafe_pop();
            }
            return ip.offset(Decoder::instr_size(Mnemonic::pop));
        }
        case Mnemonic::ret: {  // ret n
            auto n = fetch_literal(dec.operand1(ip));
            if (!n) {
                return std::unexpected(n.error());
            }
            if (*n < 0 or not m.has(*n)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            if (m.call_depth() == 0) {
                return std::unexpected(StepResult::call_stack_overrun);
            }

            for (std::int64_t i = 0; i < *n; ++i) {
                m.unsafe_pop();
            }
            return m.unsafe_pop_call();
        }
        case Mnemonic::set: {  // set i src
            auto i = dec.operand1(ip).value;
            if (i < 0 or not m.has(i + 1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto src = fetch_object(m, dec.operand2(ip));
            if (!src) {
                return std::unexpected(src.error());
            }
            m.unsafe_seek(static_cast<std::size_t>(i)) = *src;
            return ip.offset(Decoder::instr_size(Mnemonic::set));
        }
        case Mnemonic::print: {  // print
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            obj::format_to(std::cout, mgr, m.unsafe_top());
            return ip.offset(Decoder::instr_size(Mnemonic::print));
        }
        case Mnemonic::mkstr: {  // mkstr
            m.push(obj::String::make(gc, "").get());
            return ip.offset(Decoder::instr_size(Mnemonic::mkstr));
        }
        case Mnemonic::strpush: {  // strpush c
            auto c = fetch_literal(dec.operand1(ip));
            if (not c) {
                return std::unexpected(c.error());
            }
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto str = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not str or (*str)->type() != obj::DynObjType::string) {
                return std::unexpected(StepResult::mismatched_type);
            }
            static_cast<obj::String&>(**str).push(static_cast<char>(*c));
            return ip.offset(Decoder::instr_size(Mnemonic::strpush));
        }
        case Mnemonic::closure: {  // closure lam_ip
            auto lam_ip = fetch_literal(dec.operand1(ip));
            if (not lam_ip) {
                return std::unexpected(lam_ip.error());
            }
            if (*lam_ip < 0) {
                throw "unimplemented";
            }
            m.push(obj::Closure::make(gc, InstrPtr(static_cast<std::size_t>(*lam_ip))).get());
            return ip.offset(Decoder::instr_size(Mnemonic::closure));
        }
        case Mnemonic::capture: {  // capture x
            auto x = fetch_object(m, dec.operand1(ip));
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
            return ip.offset(Decoder::instr_size(Mnemonic::capture));
        }
    }

    return std::unexpected(StepResult::corrupted_opcode);
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: lispy <executable>\n";
        return EXIT_FAILURE;
    }

    Decoder dec;
    {
        std::ifstream in(argv[1], std::ios::binary);
        if (!in) {
            std::cerr << "Can't open file for reading.\n";
            return EXIT_FAILURE;
        }
        while (true) {
            auto b = in.get();
            if (not in) {
                break;
            }
            dec.bytes.push_back(static_cast<unsigned char>(b));
        }
    }

    auto ip = dec.entry_point();

    obj::NameManager mgr;
    for (std::size_t i = 8; dec.bytes[i] != '\0'; ++i) {
        std::string atom;
        while (dec.bytes[i] != '\0') {
            atom.push_back(static_cast<char>(dec.bytes[i++]));
        }
        mgr.register_(std::move(atom));
    }

    GC gc;
    Machine m;
    m.push_call(InstrPtr(dec.bytes.size()));
    while (true) {
        if (ip.value() >= dec.bytes.size()) {
            break;
        }

        auto next_ip = step(gc, mgr, m, dec, ip);
        if (not next_ip) {
            std::cout << "[Error]: " << next_ip.error() << '\n';
            return EXIT_FAILURE;
        }
        ip = *next_ip;
    }
}

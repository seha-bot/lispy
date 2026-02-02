#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "mnemonic.hpp"
#include "object.hpp"

enum class OperandType { atom, literal, stack };

struct Operand {
    OperandType type{};
    std::int64_t value{};
};

struct Instruction {
    Mnemonic mnemonic;
    Operand o1;
    Operand o2;
};

struct Machine {
    obj::Object& unsafe_seek(std::size_t i) { return m_stack.at(m_stack.size() - 1 - static_cast<std::size_t>(i)); }
    obj::Object& unsafe_top() { return unsafe_seek(0); }

    void push(obj::Object obj) { m_stack.push_back(obj); }
    void push_call(std::size_t ip) { m_return_stack.push_back(ip); }

    obj::Object unsafe_pop() {
        auto top = m_stack.back();
        m_stack.pop_back();
        return top;
    }
    std::size_t unsafe_pop_call() {
        auto ip = m_return_stack.back();
        m_return_stack.pop_back();
        return ip;
    }

    bool has(std::int64_t n) const { return n <= static_cast<std::int64_t>(m_stack.size()); }
    std::size_t call_depth() const { return m_return_stack.size(); }

    void instruct(Mnemonic mnemonic, Operand op1 = Operand{}, Operand op2 = Operand{}) {
        m_instructions.emplace_back(mnemonic, op1, op2);
    }

    Instruction const *current() const {
        if (ip >= m_instructions.size()) {
            return nullptr;
        }
        return &m_instructions[ip];
    }

private:
    std::vector<obj::Object> m_stack;
    std::vector<std::size_t> m_return_stack;
    std::vector<Instruction> m_instructions;

public:
    std::size_t ip = 0;
};

enum class StepResult {
    ok,
    ok_do_not_increment_ip,
    stack_overrun,
    call_stack_overrun,
    mismatched_type,
    halt,
};

static std::string format_sr(StepResult sr) {
    switch (sr) {
        case StepResult::ok:
            return "ok";
        case StepResult::ok_do_not_increment_ip:
            return "ok_do_not_increment_ip";
        case StepResult::stack_overrun:
            return "stack_overrun";
        case StepResult::call_stack_overrun:
            return "call_stack_overrun";
        case StepResult::mismatched_type:
            return "mismatched_type";
        case StepResult::halt:
            return "halt";
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
}

static std::expected<std::int64_t, StepResult> fetch_literal(Operand op) {
    if (op.type == OperandType::literal) {
        return op.value;
    }
    return std::unexpected(StepResult::mismatched_type);
}

// TODO: step should return the next ip wrapped in expected.
static StepResult step(GC& gc, obj::NameManager& mgr, Machine& m) {
    if (not m.current()) {
        return StepResult::halt;
    }
    auto& instruction = *m.current();

    switch (instruction.mnemonic) {
        case Mnemonic::eq: {  // eq
            if (not m.has(2)) {
                return StepResult::stack_overrun;
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_pop();
            m.push(obj::Number{arg1 == arg2});
            return StepResult::ok;
        }
        case Mnemonic::cons: {  // cons
            if (not m.has(2)) {
                return StepResult::stack_overrun;
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_top();
            m.unsafe_top() = obj::Cons::make(gc, arg1, arg2).get();
            return StepResult::ok;
        }
        case Mnemonic::car: {  // car
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto arg1 = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not arg1 or (*arg1)->type() != obj::DynObjType::cons) {
                return StepResult::mismatched_type;
            }
            m.unsafe_top() = static_cast<obj::Cons&>(**arg1).car();
            return StepResult::ok;
        }
        case Mnemonic::cdr: {  // cdr
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto arg1 = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not arg1 or (*arg1)->type() != obj::DynObjType::cons) {
                return StepResult::mismatched_type;
            }
            m.unsafe_top() = static_cast<obj::Cons&>(**arg1).cdr();
            return StepResult::ok;
        }
        case Mnemonic::jt: {  // jt offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }

            auto cond_obj = m.unsafe_pop();
            auto cond = std::get_if<obj::Number>(&cond_obj);
            if (not cond) {
                return StepResult::mismatched_type;
            }

            if ((*cond).value) {
                m.ip += static_cast<std::size_t>(*offset);
                return StepResult::ok_do_not_increment_ip;
            }
            return StepResult::ok;
        }
        case Mnemonic::jf: {  // jf offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }

            auto cond_obj = m.unsafe_pop();
            auto cond = std::get_if<obj::Number>(&cond_obj);
            if (not cond) {
                return StepResult::mismatched_type;
            }

            if (not(*cond).value) {
                m.ip += static_cast<std::size_t>(*offset);
                return StepResult::ok_do_not_increment_ip;
            }
            return StepResult::ok;
        }
        case Mnemonic::jmp: {  // jmp x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return x.error();
            }
            struct Visitor {
                auto operator()(obj::Atom) { return StepResult::mismatched_type; }
                auto operator()(obj::Number ip) {
                    m.ip = ip.value;
                    return StepResult::ok_do_not_increment_ip;
                }
                auto operator()(obj::DynObj *obj) {
                    if (obj->type() != obj::DynObjType::callable) {
                        return StepResult::mismatched_type;
                    }
                    auto& callable = static_cast<obj::Callable&>(*obj);
                    for (auto& capture : callable.captures()) {
                        m.push(capture);
                    }
                    m.ip = callable.ip();
                    return StepResult::ok_do_not_increment_ip;
                }
                Machine& m;
            };
            return std::visit(Visitor{m}, *x);
        }
        case Mnemonic::push: {  // push x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return x.error();
            }
            m.push(*x);
            return StepResult::ok;
        }
        case Mnemonic::call: {  // call x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return x.error();
            }
            struct Visitor {
                auto operator()(obj::Atom) { return StepResult::mismatched_type; }
                auto operator()(obj::Number ip) {
                    m.push_call(m.ip + 1);
                    m.ip = ip.value;
                    return StepResult::ok_do_not_increment_ip;
                }
                auto operator()(obj::DynObj *obj) {
                    if (obj->type() != obj::DynObjType::callable) {
                        return StepResult::mismatched_type;
                    }
                    auto& callable = static_cast<obj::Callable&>(*obj);
                    for (auto& capture : callable.captures()) {
                        m.push(capture);
                    }
                    m.push_call(m.ip + 1);
                    m.ip = callable.ip();
                    return StepResult::ok_do_not_increment_ip;
                }
                Machine& m;
            };
            return std::visit(Visitor{m}, *x);
        }
        case Mnemonic::pop: {  // pop n
            auto n = fetch_literal(instruction.o1);
            if (!n) {
                return n.error();
            }
            if (*n < 0 or not m.has(*n)) {
                return StepResult::stack_overrun;
            }
            for (std::int64_t i = 0; i < *n; ++i) {
                m.unsafe_pop();
            }
            return StepResult::ok;
        }
        case Mnemonic::ret: {  // ret n
            auto n = fetch_literal(instruction.o1);
            if (!n) {
                return n.error();
            }
            if (*n < 0 or not m.has(*n)) {
                return StepResult::stack_overrun;
            }
            if (m.call_depth() == 0) {
                return StepResult::call_stack_overrun;
            }

            for (std::int64_t i = 0; i < *n; ++i) {
                m.unsafe_pop();
            }
            m.ip = m.unsafe_pop_call();
            return StepResult::ok_do_not_increment_ip;
        }
        case Mnemonic::set: {  // set i src
            auto i = fetch_literal(instruction.o1);
            if (!i) {
                return i.error();
            }
            if (*i < 0 or not m.has(*i + 1)) {
                return StepResult::stack_overrun;
            }
            auto src = fetch_object(m, instruction.o2);
            if (!src) {
                return src.error();
            }
            m.unsafe_seek(static_cast<std::size_t>(*i)) = *src;
            return StepResult::ok;
        }
        case Mnemonic::halt:  // halt
            return StepResult::halt;
        case Mnemonic::print: {  // print
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            obj::format_to(std::cout, mgr, m.unsafe_top());
            return StepResult::ok;
        }
        case Mnemonic::mkstr: {  // mkstr
            m.push(obj::String::make(gc, "").get());
            return StepResult::ok;
        }
        case Mnemonic::strpush: {  // strpush c
            auto c = fetch_literal(instruction.o1);
            if (not c) {
                return c.error();
            }
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto str = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not str or (*str)->type() != obj::DynObjType::string) {
                return StepResult::mismatched_type;
            }
            static_cast<obj::String&>(**str).push(static_cast<char>(*c));
            return StepResult::ok;
        }
        case Mnemonic::closure: {  // closure ip
            auto ip = fetch_literal(instruction.o1);
            if (not ip) {
                return ip.error();
            }
            // TODO: fix this (and others) unsafe implicit conversion
            std::size_t warning = *ip;
            m.push(obj::Callable::make(gc, warning).get());
            return StepResult::ok;
        }
        case Mnemonic::capture: {  // capture x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return x.error();
            }
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto obj = std::get_if<obj::DynObj *>(&m.unsafe_top());
            if (not obj or (*obj)->type() != obj::DynObjType::callable) {
                return StepResult::mismatched_type;
            }
            static_cast<obj::Callable&>(**obj).capture(*x);
            return StepResult::ok;
        }
    }
}

static void execute(GC& gc, obj::NameManager& mgr, Machine& m) {
    while (true) {
        StepResult sr = step(gc, mgr, m);
        if (sr == StepResult::halt) {
            break;
        }
        if (sr != StepResult::ok and sr != StepResult::ok_do_not_increment_ip) {
            std::cout << "[Error] at " << m.ip << ": " << format_sr(sr) << '\n';
            break;
        }
        if (sr != StepResult::ok_do_not_increment_ip) {
            m.ip += 1;
        }
    }
}

int main() {
    GC gc;
    obj::NameManager mgr;

    // auto c = as.register_("C");
    // auto b = as.register_("B");
    // auto a = as.register_("A");

    Machine m;

    execute(gc, mgr, m);
}

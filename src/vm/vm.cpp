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

    InstrPtr ip() const { return m_ip; }
    void jump(InstrPtr ip) { m_ip = ip; }

    Instruction const *current() const {
        if (m_ip.value() >= m_instructions.size()) {
            return nullptr;
        }
        return &m_instructions[m_ip.value()];
    }

private:
    std::vector<obj::Object> m_stack;
    std::vector<InstrPtr> m_return_stack;
    std::vector<Instruction> m_instructions;
    InstrPtr m_ip{0};
};

enum class StepResult {
    stack_overrun,
    call_stack_overrun,
    mismatched_type,
};

static std::string format_sr(StepResult sr) {
    switch (sr) {
        case StepResult::stack_overrun:
            return "stack_overrun";
        case StepResult::call_stack_overrun:
            return "call_stack_overrun";
        case StepResult::mismatched_type:
            return "mismatched_type";
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
static std::expected<InstrPtr, StepResult> step(GC& gc, obj::NameManager& mgr, Machine& m, Instruction instruction) {
    switch (instruction.mnemonic) {
        case Mnemonic::eq: {  // eq
            if (not m.has(2)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_pop();
            m.push(obj::Number{arg1 == arg2});
            return m.ip().next();
        }
        case Mnemonic::cons: {  // cons
            if (not m.has(2)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_top();
            m.unsafe_top() = obj::Cons::make(gc, arg1, arg2).get();
            return m.ip().next();
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
            return m.ip().next();
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
            return m.ip().next();
        }
        case Mnemonic::jt: {  // jt offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
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
                return m.ip().offset(*offset);
            }
            return m.ip().next();
        }
        case Mnemonic::jf: {  // jf offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
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
                return m.ip().offset(*offset);
            }
            return m.ip().next();
        }
        case Mnemonic::jmp: {  // jmp offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
                return std::unexpected(offset.error());
            }
            return m.ip().offset(*offset);
        }
        case Mnemonic::call: {  // call offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
                return std::unexpected(offset.error());
            }
            m.push_call(m.ip().next());
            return m.ip().offset(*offset);
        }
        case Mnemonic::indjmp: {  // indjmp x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return std::unexpected(x.error());
            }
            struct Visitor {
                std::expected<InstrPtr, StepResult> operator()(obj::Atom) {
                    return std::unexpected(StepResult::mismatched_type);
                }
                std::expected<InstrPtr, StepResult> operator()(obj::Number offset) {
                    // TODO: ???
                    return m.ip().offset(offset.value);
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
            };
            return std::visit(Visitor{m}, *x);
        }
        case Mnemonic::indcall: {  // indcall x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return std::unexpected(x.error());
            }
            struct Visitor {
                std::expected<InstrPtr, StepResult> operator()(obj::Atom) {
                    return std::unexpected(StepResult::mismatched_type);
                }
                std::expected<InstrPtr, StepResult> operator()(obj::Number offset) {
                    m.push_call(m.ip().next());
                    // TODO: ???
                    return m.ip().offset(offset.value);
                }
                std::expected<InstrPtr, StepResult> operator()(obj::DynObj *obj) {
                    if (obj->type() != obj::DynObjType::closure) {
                        return std::unexpected(StepResult::mismatched_type);
                    }
                    auto& closure = static_cast<obj::Closure&>(*obj);
                    for (auto& capture : closure.captures()) {
                        m.push(capture);
                    }
                    m.push_call(m.ip().next());
                    return closure.ip();
                }
                Machine& m;
            };
            return std::visit(Visitor{m}, *x);
        }
        case Mnemonic::push: {  // push x
            auto x = fetch_object(m, instruction.o1);
            if (!x) {
                return std::unexpected(x.error());
            }
            m.push(*x);
            return m.ip().next();
        }
        case Mnemonic::pop: {  // pop n
            auto n = fetch_literal(instruction.o1);
            if (!n) {
                return std::unexpected(n.error());
            }
            if (*n < 0 or not m.has(*n)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            for (std::int64_t i = 0; i < *n; ++i) {
                m.unsafe_pop();
            }
            return m.ip().next();
        }
        case Mnemonic::ret: {  // ret n
            auto n = fetch_literal(instruction.o1);
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
            auto i = fetch_literal(instruction.o1);
            if (!i) {
                return std::unexpected(i.error());
            }
            if (*i < 0 or not m.has(*i + 1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            auto src = fetch_object(m, instruction.o2);
            if (!src) {
                return std::unexpected(src.error());
            }
            m.unsafe_seek(static_cast<std::size_t>(*i)) = *src;
            return m.ip().next();
        }
        case Mnemonic::print: {  // print
            if (not m.has(1)) {
                return std::unexpected(StepResult::stack_overrun);
            }
            obj::format_to(std::cout, mgr, m.unsafe_top());
            return m.ip().next();
        }
        case Mnemonic::mkstr: {  // mkstr
            m.push(obj::String::make(gc, "").get());
            return m.ip().next();
        }
        case Mnemonic::strpush: {  // strpush c
            auto c = fetch_literal(instruction.o1);
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
            return m.ip().next();
        }
        case Mnemonic::closure: {  // closure ip
            auto ip = fetch_literal(instruction.o1);
            if (not ip) {
                return std::unexpected(ip.error());
            }
            if (*ip < 0) {
                throw "unimplemented";
            }
            m.push(obj::Closure::make(gc, InstrPtr(static_cast<std::size_t>(*ip))).get());
            return m.ip().next();
        }
        case Mnemonic::capture: {  // capture x
            auto x = fetch_object(m, instruction.o1);
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
            return m.ip().next();
        }
    }
}

static void execute(GC& gc, obj::NameManager& mgr, Machine& m) {
    while (true) {
        auto instruction = m.current();
        if (not instruction) {
            break;
        }

        auto ip = step(gc, mgr, m, *instruction);
        if (not ip) {
            std::cout << "[Error] at " << m.ip().value() << ": " << format_sr(ip.error()) << '\n';
        }
        m.jump(*ip);
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

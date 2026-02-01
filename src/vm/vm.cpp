#include <cstddef>
#include <cstdint>
#include <expected>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "gc.hpp"
#include "mnemonic.hpp"

enum class OperandType {
    literal,
    atom,
    stack,
};

struct Operand {
    OperandType type{};
    std::int64_t value{};
};

struct Instruction {
    Mnemonic mnemonic;
    Operand o1;
    Operand o2;
};

struct Object : GC::Node {
    virtual ~Object() = default;
    virtual bool is_atom() = 0;
    virtual bool is_cons() = 0;
    virtual std::string format(struct AtomStorage const& as, bool parens = true) = 0;
};

struct Atom : Object, GC::Managed<Atom> {
    bool is_atom() override { return true; }
    bool is_cons() override { return false; }
    std::string format(struct AtomStorage const& as, bool parens = true) override;
    std::size_t id() const { return m_id; }

    Atom(std::size_t id) : m_id(id) {}

private:
    friend GC::Managed<Atom>;
    std::size_t m_id;
};

struct Cons : Object, GC::Managed<Cons> {
    bool is_atom() override { return false; }
    bool is_cons() override { return true; }
    std::string format(struct AtomStorage const& as, bool parens = true) override;
    Object *car() const { return m_car; }
    Object *cdr() const { return m_cdr; }

private:
    friend GC::Managed<Cons>;
    Cons(Object *car, Object *cdr) : m_car(car), m_cdr(cdr) {
        depend_on(car);
        depend_on(cdr);
    }
    Object *m_car, *m_cdr;
};

// TODO: rename this to Object and rename former Object to GCObject
struct StackElement {
    // TODO: rename value to number
    union {
        std::int64_t value;
        Object *object;
    };
    enum class Type { value, object } type;
};

struct Machine {
    StackElement& unsafe_seek(std::int64_t i) { return m_stack[m_stack.size() - 1 - static_cast<std::size_t>(i)]; }
    StackElement& unsafe_top() { return m_stack.back(); }

    StackElement unsafe_pop() {
        auto top = m_stack.back();
        m_stack.pop_back();
        return top;
    }

    void push(Object *obj) { m_stack.push_back(StackElement{.object = obj, .type = StackElement::Type::object}); }
    void push(std::int64_t val) { m_stack.push_back(StackElement{.value = val, .type = StackElement::Type::value}); }

    bool has(std::int64_t n) const { return n <= static_cast<std::int64_t>(m_stack.size()); }

    bool unsafe_seek_is_object(std::int64_t i) { return unsafe_seek(i).type == StackElement::Type::object; }

    void push_call(std::size_t ip) { m_return_stack.push_back(ip); }

    std::size_t unsafe_pop_call() {
        auto ip = m_return_stack.back();
        m_return_stack.pop_back();
        return ip;
    }

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
    std::vector<StackElement> m_stack;
    std::vector<std::size_t> m_return_stack;
    std::vector<Instruction> m_instructions;

public:
    std::size_t ip = 0;
};

struct AtomStorage {
    AtomStorage(GC& gc) : m_gc(gc) {
        register_("F");
        register_("T");
        register_("NIL");
    }

    AtomStorage(AtomStorage const&) = delete;
    AtomStorage& operator=(AtomStorage const&) = delete;

    static constexpr std::size_t false_ = 0;
    static constexpr std::size_t true_ = 1;
    static constexpr std::size_t nil = 2;

    // TODO: rename to allocate or something better
    std::size_t register_(std::string atom) {
        m_atoms.push_back(Atom::make(m_gc, m_atoms.size()));
        m_atom_names.push_back(std::move(atom));
        return m_atoms.size() - 1;
    }

    // NOTE: we assume that all atom ids are correct since they're literals anyways.
    Atom *get(std::int64_t id) { return m_atoms[static_cast<std::size_t>(id)].get(); }
    std::string const& get_name(std::size_t id) const { return m_atom_names[id]; }

private:
    GC& m_gc;
    std::vector<GC::Ptr<Atom>> m_atoms;
    std::vector<std::string> m_atom_names;
};

enum class StepResult {
    ok,
    ok_do_not_increment_ip,
    stack_overrun,
    call_stack_overrun,
    mismatched_type,
    invalid_return_address,
    halt,
};

std::string format_sr(StepResult sr) {
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
        case StepResult::invalid_return_address:
            return "invalid_return_address";
        case StepResult::halt:
            return "halt";
    }
}

std::expected<Object *, StepResult> fetch_object(AtomStorage& as, Machine& m, Operand op) {
    switch (op.type) {
        case OperandType::literal:
            return std::unexpected(StepResult::mismatched_type);
        case OperandType::atom:
            return as.get(op.value);
        case OperandType::stack: {
            auto& obj = m.unsafe_seek(op.value);
            if (obj.type != StackElement::Type::object) {
                return std::unexpected(StepResult::stack_overrun);
            }
            return obj.object;
        }
    }
}

std::expected<std::int64_t, StepResult> fetch_literal(Operand op) {
    switch (op.type) {
        case OperandType::literal:
            return op.value;
        case OperandType::atom:
        case OperandType::stack:
            return std::unexpected(StepResult::mismatched_type);
    }
}

StepResult step(GC& gc, AtomStorage& as, Machine& m) {
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
            if (arg1.type != arg2.type) {
                m.push(std::int64_t(0));
                return StepResult::ok;
            }
            switch (arg1.type) {
                case StackElement::Type::value:
                    m.push(arg1.value == arg2.value ? 1 : 0);
                    return StepResult::ok;
                case StackElement::Type::object:
                    m.push(arg1.object == arg2.object && arg1.object->is_atom() ? 1 : 0);
                    return StepResult::ok;
            }
        }
        case Mnemonic::cons: {  // cons
            if (not m.has(2)) {
                return StepResult::stack_overrun;
            }
            if (not m.unsafe_seek_is_object(0) or not m.unsafe_seek_is_object(1)) {
                return StepResult::mismatched_type;
            }
            auto arg1 = m.unsafe_pop();
            auto arg2 = m.unsafe_top();
            m.unsafe_top().object = Cons::make(gc, arg1.object, arg2.object).get();
            return StepResult::ok;
        }
        case Mnemonic::car: {  // car
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto& arg1 = m.unsafe_top();
            if (arg1.type != StackElement::Type::object or not arg1.object->is_cons()) {
                return StepResult::mismatched_type;
            }
            m.unsafe_top().object = static_cast<Cons&>(*arg1.object).car();
            return StepResult::ok;
        }
        case Mnemonic::cdr: {  // cdr
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto& arg1 = m.unsafe_top();
            if (arg1.type != StackElement::Type::object or not arg1.object->is_cons()) {
                return StepResult::mismatched_type;
            }
            m.unsafe_top().object = static_cast<Cons&>(*arg1.object).cdr();
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

            auto b = m.unsafe_pop();
            if (b.type != StackElement::Type::value) {
                return StepResult::mismatched_type;
            }

            if (b.value) {
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

            auto b = m.unsafe_pop();
            if (b.type != StackElement::Type::value) {
                return StepResult::mismatched_type;
            }

            if (not b.value) {
                m.ip += static_cast<std::size_t>(*offset);
                return StepResult::ok_do_not_increment_ip;
            }
            return StepResult::ok;
        }
        case Mnemonic::jmp: {  // jmp offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            m.ip += static_cast<std::size_t>(*offset);
            return StepResult::ok_do_not_increment_ip;
        }
        case Mnemonic::push: {  // push x
            auto x = fetch_object(as, m, instruction.o1);
            if (!x) {
                return x.error();
            }
            m.push(*x);
            return StepResult::ok;
        }
        case Mnemonic::call: {  // call offset
            auto offset = fetch_literal(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            m.push_call(m.ip + 1);
            m.ip += static_cast<std::size_t>(*offset);
            return StepResult::ok_do_not_increment_ip;
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
            if (*i < 0 or not m.has(*i)) {
                return StepResult::stack_overrun;
            }
            auto src = fetch_object(as, m, instruction.o2);
            if (!src) {
                return src.error();
            }
            m.unsafe_seek(*i).object = *src;
            return StepResult::ok;
        }
        case Mnemonic::halt:  // halt
            return StepResult::halt;
        case Mnemonic::print: {  // print
            if (not m.has(1)) {
                return StepResult::stack_overrun;
            }
            auto top = m.unsafe_seek(0);
            if (top.type == StackElement::Type::value) {
                std::cout << top.value;
            } else {
                std::cout << top.object->format(as);
            }
            return StepResult::ok;
        }
    }
}

void execute(GC& gc, AtomStorage& as, Machine& m) {
    while (true) {
        StepResult sr = step(gc, as, m);
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
    AtomStorage as(gc);

    // auto c = as.register_("C");
    // auto b = as.register_("B");
    // auto a = as.register_("A");

    Machine m;

    m.instruct(Mnemonic::push, Operand{OperandType::atom, AtomStorage::true_});
    m.instruct(Mnemonic::call, Operand{OperandType::literal, 6});
    m.instruct(Mnemonic::jf, Operand{OperandType::literal, 3});
    m.instruct(Mnemonic::print, Operand{OperandType::atom, AtomStorage::true_});
    m.instruct(Mnemonic::halt);
    m.instruct(Mnemonic::print, Operand{OperandType::atom, AtomStorage::false_});
    m.instruct(Mnemonic::halt);

    // null
    m.instruct(Mnemonic::push, Operand{OperandType::atom, AtomStorage::nil});
    m.instruct(Mnemonic::eq);
    m.instruct(Mnemonic::ret, Operand{OperandType::literal, 0});

    execute(gc, as, m);
}

std::string Atom::format(struct AtomStorage const& as, bool) { return as.get_name(m_id); }

std::string Cons::format(struct AtomStorage const& as, bool parens) {
    std::string r;
    if (parens) {
        r += '(';
    }
    r += m_car->format(as, true);
    if (not m_cdr->is_atom() or static_cast<Atom *>(m_cdr)->id() != 2) {
        r += ' ' + m_cdr->format(as, false);
    }
    if (parens) {
        r += ')';
    }
    return r;
}

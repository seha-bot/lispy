#include <cstddef>
#include <expected>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "gc.hpp"

enum class Mnemonic {
    atom,
    eq,
    cons,
    car,
    cdr,
    jt,
    jf,
    jmp,
    call,
    ret,
    push,
    pop,
    set,
    halt,
    print,
};

enum class OperandType {
    register_,
    stack,
    atom,
    number,
};

struct Operand {
    OperandType type;
    std::size_t value;
};

struct Instruction {
    Mnemonic mnemonic;
    Operand o1;
    Operand o2;
    Operand o3;
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

struct Machine {
    std::vector<Instruction> instructions;
    std::vector<Object *> object_stack;
    std::vector<std::size_t> number_stack;
    Object *a{}, *b{}, *c{};
    std::size_t ip = 0;
    bool sf = false;

    Object *& get_register(std::size_t id) {
        switch (id) {
            case 0:
                return a;
            case 1:
                return b;
            case 2:
                return c;
            default:
                std::unreachable();
        }
    }

    Object *object_stack_get(std::size_t i) {
        if (object_stack.size() - 1 - i < 0) {
            return nullptr;
        }
        return object_stack[object_stack.size() - 1 - i];
    }
};

struct AtomStorage {
    AtomStorage(GC& gc) : m_gc(gc) {
        register_("F");
        register_("T");
        register_("NIL");
    }

    static constexpr std::size_t false_ = 0;
    static constexpr std::size_t true_ = 1;
    static constexpr std::size_t nil = 2;

    std::size_t register_(std::string atom) {
        m_atoms.push_back(Atom::make(m_gc, m_atoms.size()));
        m_atom_names.push_back(std::move(atom));
        return m_atoms.size() - 1;
    }

    Atom *get(std::size_t id) { return m_atoms[id].get(); }
    std::string const& get_name(std::size_t id) const { return m_atom_names[id]; }

private:
    GC& m_gc;
    std::vector<GC::Ptr<Atom>> m_atoms;
    std::vector<std::string> m_atom_names;
};

enum class StepResult {
    ok,
    stack_overrun,
    mismatched_type,
    halt,
};

std::string format_sr(StepResult sr) {
    switch (sr) {
        case StepResult::ok:
            return "ok";
        case StepResult::stack_overrun:
            return "stack_overrun";
        case StepResult::mismatched_type:
            return "mismatched_type";
        case StepResult::halt:
            return "halt";
    }
}

Atom *boolean_test(AtomStorage& as, Machine& m, bool b) {
    m.sf = b;
    return as.get(b ? AtomStorage::true_ : AtomStorage::false_);
}

std::expected<Object *, StepResult> fetch_object(AtomStorage& as, Machine& m, Operand op) {
    switch (op.type) {
        case OperandType::register_:
            return m.get_register(op.value);
        case OperandType::stack:
            if (auto *obj = m.object_stack_get(op.value)) {
                return obj;
            }
            return std::unexpected(StepResult::stack_overrun);
        case OperandType::atom:
            return as.get(op.value);
        case OperandType::number:
            return std::unexpected(StepResult::mismatched_type);
    }
}

std::expected<std::size_t, StepResult> fetch_atom_id(Machine& m, Operand op) {
    switch (op.type) {
        case OperandType::register_: {
            auto *reg = m.get_register(op.value);
            if (reg->is_atom()) {
                return static_cast<Atom&>(*reg).id();
            } else {
                return std::unexpected(StepResult::mismatched_type);
            }
        }
        case OperandType::stack:
            if (auto *obj = m.object_stack_get(op.value)) {
                if (obj->is_atom()) {
                    return static_cast<Atom&>(*obj).id();
                } else {
                    return std::unexpected(StepResult::mismatched_type);
                }
            }
            return std::unexpected(StepResult::stack_overrun);
        case OperandType::atom:
            return op.value;
        case OperandType::number:
            return std::unexpected(StepResult::mismatched_type);
    }
}

std::expected<Cons *, StepResult> fetch_cons(Machine& m, Operand op) {
    switch (op.type) {
        case OperandType::register_: {
            auto *reg = m.get_register(op.value);
            if (reg->is_cons()) {
                return static_cast<Cons *>(reg);
            } else {
                return std::unexpected(StepResult::mismatched_type);
            }
        }
        case OperandType::stack:
            if (auto obj = m.object_stack_get(op.value)) {
                if (obj->is_cons()) {
                    return static_cast<Cons *>(obj);
                } else {
                    return std::unexpected(StepResult::mismatched_type);
                }
            }
            return std::unexpected(StepResult::stack_overrun);
        case OperandType::atom:
        case OperandType::number:
            return std::unexpected(StepResult::mismatched_type);
    }
}

std::expected<std::size_t, StepResult> fetch_number(Operand op) {
    switch (op.type) {
        case OperandType::register_:
        case OperandType::stack:
        case OperandType::atom:
            return std::unexpected(StepResult::mismatched_type);
        case OperandType::number:
            return op.value;
    }
}

StepResult step(GC& gc, AtomStorage& as, Machine& m) {
    auto& instruction = m.instructions[m.ip];
    switch (instruction.mnemonic) {
        case Mnemonic::atom: {  // atom dest x
            auto& dest = m.get_register(instruction.o1.value);
            auto& arg = instruction.o2;
            switch (arg.type) {
                case OperandType::register_:
                    dest = boolean_test(as, m, m.get_register(arg.value)->is_atom());
                    return StepResult::ok;
                case OperandType::stack: {
                    auto obj = m.object_stack_get(arg.value);
                    if (!obj) {
                        return StepResult::stack_overrun;
                    }
                    dest = boolean_test(as, m, obj->is_atom());
                    return StepResult::ok;
                }
                case OperandType::atom:
                    dest = boolean_test(as, m, true);
                    return StepResult::ok;
                case OperandType::number:
                    return StepResult::mismatched_type;
            }
        }
        case Mnemonic::eq: {  // eq dest x y
            auto& dest = m.get_register(instruction.o1.value);
            auto arg1_id = fetch_atom_id(m, instruction.o2);
            if (!arg1_id) {
                return arg1_id.error();
            }
            auto arg2_id = fetch_atom_id(m, instruction.o3);
            if (!arg2_id) {
                return arg1_id.error();
            }
            dest = boolean_test(as, m, arg1_id == arg2_id);
        }
        case Mnemonic::cons: {  // cons dest x xs
            auto& dest = m.get_register(instruction.o1.value);
            auto x = fetch_object(as, m, instruction.o2);
            if (!x) {
                return x.error();
            }
            auto xs = fetch_object(as, m, instruction.o3);
            if (!xs) {
                return xs.error();
            }
            dest = Cons::make(gc, *x, *xs).get();
            return StepResult::ok;
        }
        case Mnemonic::car: {  // car dest x
            auto& dest = m.get_register(instruction.o1.value);
            auto x = fetch_cons(m, instruction.o2);
            if (!x) {
                return x.error();
            }
            dest = (*x)->car();
            return StepResult::ok;
        }
        case Mnemonic::cdr: {  // cdr dest x
            auto& dest = m.get_register(instruction.o1.value);
            auto x = fetch_cons(m, instruction.o2);
            if (!x) {
                return x.error();
            }
            dest = (*x)->cdr();
            return StepResult::ok;
        }
        case Mnemonic::jt: {  // jt offset
            auto offset = fetch_number(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            if (m.sf) {
                m.ip += *offset;
            }
            return StepResult::ok;
        }
        case Mnemonic::jf: {  // jf offset
            auto offset = fetch_number(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            if (not m.sf) {
                m.ip += *offset;
            }
            return StepResult::ok;
        }
        case Mnemonic::jmp: {  // jmp offset
            auto offset = fetch_number(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            m.ip += *offset;
            return StepResult::ok;
        }
        case Mnemonic::call: {  // call offset
            auto offset = fetch_number(instruction.o1);
            if (!offset) {
                return offset.error();
            }
            m.number_stack.push_back(m.ip + 1);
            m.ip += *offset;
            return StepResult::ok;
        }
        case Mnemonic::ret: {  // ret n
            auto n = fetch_number(instruction.o1);
            if (!n) {
                return n.error();
            }
            if (m.number_stack.size() < *n + 1) {
                return StepResult::stack_overrun;
            }
            m.ip = m.number_stack.back();
            for (std::size_t i = 0; i <= *n; ++i) {
                m.number_stack.pop_back();
            }
            return StepResult::ok;
        }
        case Mnemonic::push: {  // push x
            auto x = fetch_object(as, m, instruction.o1);
            if (!x) {
                return x.error();
            }
            m.object_stack.push_back(*x);
            return StepResult::ok;
        }
        case Mnemonic::pop: {  // pop dest
            auto& dest = m.get_register(instruction.o1.value);
            if (m.object_stack.empty()) {
                return StepResult::stack_overrun;
            }
            dest = m.object_stack.back();
            m.object_stack.pop_back();
            return StepResult::ok;
        }
        case Mnemonic::set: {  // set dest x
            auto& dest = m.get_register(instruction.o1.value);
            auto x = fetch_object(as, m, instruction.o2);
            if (!x) {
                return x.error();
            }
            dest = *x;
            return StepResult::ok;
        }
        case Mnemonic::halt:  // halt
            return StepResult::halt;
        case Mnemonic::print: {  // print x
            auto x = fetch_object(as, m, instruction.o1);
            if (!x) {
                return x.error();
            }
            std::cout << (*x)->format(as);
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
        if (sr != StepResult::ok) {
            std::cout << "Error: " << format_sr(sr) << '\n';
            break;
        }
        m.ip += 1;
    }
}

int main() {
    GC gc;
    AtomStorage as(gc);

    Machine m;
    m.instructions.emplace_back(Mnemonic::print, Operand{OperandType::atom, AtomStorage::nil});
    m.instructions.emplace_back(Mnemonic::halt);

    // null
    m.instructions.emplace_back(Mnemonic::atom, Operand{OperandType::register_, 0}, Operand{OperandType::stack, 1});
    m.instructions.emplace_back(Mnemonic::jf, Operand{OperandType::number, 2});
    m.instructions.emplace_back(Mnemonic::atom, Operand{OperandType::register_, 0}, Operand{OperandType::stack, 1},
                                Operand{OperandType::atom, AtomStorage::nil});
    m.instructions.emplace_back(Mnemonic::ret, Operand{OperandType::number, 1});

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
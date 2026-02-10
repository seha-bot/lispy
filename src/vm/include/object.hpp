#ifndef OBJECT_HPP
#define OBJECT_HPP

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "gc.hpp"
#include "instr_ptr.hpp"

namespace obj {

struct Atom {
    bool operator==(Atom const&) const = default;
    std::size_t id;
};
struct Number {
    bool operator==(Number const&) const = default;
    std::int64_t value;
};

using Object = std::variant<Atom, Number, struct DynObj *>;

struct NameManager {
    NameManager() = default;
    NameManager(NameManager const&) = delete;
    NameManager& operator=(NameManager const&) = delete;

    std::size_t register_(std::string atom) {
        m_names.push_back(std::move(atom));
        return m_names.size() - 1;
    }

    void assign(std::vector<std::string> names) { m_names = std::move(names); }

    std::string const& get_name(obj::Atom atom) const { return m_names[atom.id]; }

private:
    std::vector<std::string> m_names;
};

void format_to(std::ostream& os, NameManager& mgr, Object obj);

enum class DynObjType {
    closure,
    cons,
    string,
};

struct DynObj : GC::Node {
    virtual DynObjType type() const = 0;
    virtual void format_to(std::ostream& os, NameManager& mgr) const = 0;
};

struct Closure : DynObj, GC::Managed<Closure> {
    DynObjType type() const override { return DynObjType::closure; }
    void format_to(std::ostream& os, NameManager&) const override { os << "<closure>"; }

    std::vector<Object> const& captures() const { return m_captures; }
    InstrPtr ip() const { return m_ip; }

    void capture(Object obj) { return m_captures.push_back(obj); }

private:
    friend GC::Managed<Closure>;
    Closure(InstrPtr ip) : m_ip(ip) {}

    std::vector<Object> m_captures;
    InstrPtr m_ip;
};

struct Cons : DynObj, GC::Managed<Cons> {
    DynObjType type() const override { return DynObjType::cons; }

    void format_to(std::ostream& os, NameManager& mgr) const override {
        os << '(';
        obj::format_to(os, mgr, m_car);

        Object it = m_cdr;
        while (auto obj = std::get_if<DynObj *>(&it)) {
            if ((*obj)->type() != DynObjType::cons) {
                break;
            }
            auto cons = static_cast<Cons&>(**obj);
            obj::format_to(os << ' ', mgr, cons.m_car);
            it = cons.m_cdr;
        }
        if (auto atom = std::get_if<Atom>(&it); not atom or mgr.get_name(*atom) != "NIL") {
            obj::format_to(os << ' ', mgr, it);
        }
        os << ')';
    }

    Object car() const { return m_car; }
    Object cdr() const { return m_cdr; }

private:
    friend GC::Managed<Cons>;
    Cons(Object car, Object cdr) : m_car(car), m_cdr(cdr) {
        if (auto obj = std::get_if<DynObj *>(&car)) {
            depend_on(*obj);
        }
        if (auto obj = std::get_if<DynObj *>(&cdr)) {
            depend_on(*obj);
        }
    }

    Object m_car, m_cdr;
};

struct String : DynObj, GC::Managed<String> {
    DynObjType type() const override { return DynObjType::string; }
    void format_to(std::ostream& os, NameManager&) const override { os << m_value; }

    std::string const& value() const { return m_value; }

    void push(char c) { m_value.push_back(c); }

private:
    friend GC::Managed<String>;
    String(std::string value) : m_value(std::move(value)) {}

    std::string m_value;
};

inline void format_to(std::ostream& os, NameManager& mgr, Object obj) {
    struct Visitor {
        void operator()(Atom atom) { os << mgr.get_name(atom); }
        void operator()(Number num) { os << num.value; }
        void operator()(DynObj *obj) { obj->format_to(os, mgr); }
        std::ostream& os;
        NameManager& mgr;
    };
    std::visit(Visitor{os, mgr}, obj);
}

}  // namespace obj

#endif

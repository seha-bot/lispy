#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gc.hpp"

namespace ast {

struct Expr : GC::Node {
    virtual ~Expr() = default;
    virtual std::string format(bool = true) const = 0;
};

class ExprAtom : public Expr, public GC::Managed<ExprAtom> {
    friend GC::Managed<ExprAtom>;
    ExprAtom(std::string value) : value(std::move(value)) {}

public:
    // static GC::Ptr<ExprAtom> make(GC& gc, std::string s) {
    //     GC::Ptr<ExprAtom> p(new ExprAtom(std::move(s)));
    //     gc.register_(p.get());
    //     return p;
    // }

    std::string format(bool = true) const override { return value; }

    std::string value;
};

class ExprCons : public Expr, public GC::Managed<ExprCons> {
    friend GC::Managed<ExprCons>;
    ExprCons(Expr *car, Expr *cdr) : car(car), cdr(cdr) {
        depend_on(car);
        depend_on(cdr);
    }

public:
    Expr *car, *cdr;

    std::string format(bool parens = true) const override {
        std::string r;
        if (parens) {
            r += '(';
        }
        r += car->format(true);
        if (auto *e = dynamic_cast<ExprAtom *>(cdr); not e or e->value != "NIL") {
            r += ' ' + cdr->format(false);
        }
        if (parens) {
            r += ')';
        }
        return r;
    }
};

Expr *car_(Expr *e) { return dynamic_cast<ExprCons&>(*e).car; }
Expr *cdr_(Expr *e) { return dynamic_cast<ExprCons&>(*e).cdr; }
bool is_atom(Expr *e) { return dynamic_cast<ExprAtom *>(e) != nullptr; }

}  // namespace ast

#endif

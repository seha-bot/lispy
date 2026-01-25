#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace ast {

struct Expr {
    virtual ~Expr() = default;
    virtual std::string format(bool = true) const = 0;
};

struct ExprAtom : Expr {
    std::string value;
    ExprAtom(std::string value) : value(std::move(value)) {}

    std::string format(bool = true) const override { return value; }
};

struct ExprCons : Expr {
    Expr *car, *cdr;
    ExprCons(Expr *car, Expr *cdr) : car(car), cdr(cdr) {}

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

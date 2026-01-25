#ifndef ALLOC_HPP
#define ALLOC_HPP

#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "ast.hpp"

// Currently, ATOMs are never collected.

struct Alloc {
    Alloc(GC& gc) : m_gc(gc) {
        atom("NIL")->lock();
        atom("t")->lock();
        atom("f")->lock();
    }

    ast::ExprAtom *atom(std::string value) {
        for (auto x : m_atoms) {
            if (x->value == value) {
                return x;
            }
        }
        auto p = ast::ExprAtom::make(m_gc, std::move(value)).get();
        p->lock();
        m_atoms.push_back(p);
        return p;
    }

    ast::ExprCons *cons(ast::Expr *a, ast::Expr *b) { return ast::ExprCons::make(m_gc, a, b).get(); }
    ast::ExprAtom *nil() { return m_atoms[0]; }
    ast::ExprAtom *true_() { return m_atoms[1]; }
    ast::ExprAtom *false_() { return m_atoms[2]; }

private:
    GC& m_gc;
    std::vector<ast::ExprAtom *> m_atoms;
};

#endif

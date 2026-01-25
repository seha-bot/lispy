#ifndef GC_HPP
#define GC_HPP

#include <string>
#include <utility>
#include <vector>

#include "ast.hpp"

struct GC {
    std::vector<ast::ExprAtom *> atom_storage{new ast::ExprAtom("NIL"), new ast::ExprAtom("t"), new ast::ExprAtom("f")};
    std::vector<ast::ExprCons *> cons_storage;
    ast::ExprAtom *alloc_atom(std::string value) {
        for (auto x : atom_storage) {
            if (x->value == value) {
                return x;
            }
        }
        return atom_storage.emplace_back(new ast::ExprAtom(std::move(value)));
    }
    ast::ExprCons *alloc_cons(ast::Expr *a, ast::Expr *b) { return cons_storage.emplace_back(new ast::ExprCons(a, b)); }
    ast::ExprAtom *nil() { return atom_storage[0]; }
    ast::ExprAtom *true_() { return atom_storage[1]; }
    ast::ExprAtom *false_() { return atom_storage[2]; }

    // void mark(Expr *expr);
    // void sweep();

    ~GC() {
        for (auto x : atom_storage) delete x;
        for (auto x : cons_storage) delete x;
    }
};

#endif

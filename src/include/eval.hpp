#ifndef EVAL_HPP
#define EVAL_HPP

#include <stdexcept>

#include "ast.hpp"
#include "gc.hpp"

using Env = std::unordered_map<std::string_view, ast::Expr *>;

ast::Expr *eval(ast::Expr const *e, Env& env, GC& gc) {
    if (auto *a = dynamic_cast<ast::ExprAtom const *>(e)) {
        return env.at(a->value);
    }

    auto *car = dynamic_cast<ast::ExprCons const&>(*e).car;
    auto *cdr = dynamic_cast<ast::ExprCons const&>(*e).cdr;

    auto check = [&gc](bool b) { return b ? gc.true_() : gc.false_(); };

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car)) {
        if (a->value == "QUOTE") {
            return car_(cdr);
        } else if (a->value == "ATOM") {
            auto expr = eval(car_(cdr), env, gc);
            return check(is_atom(expr));
        } else if (a->value == "EQ") {
            auto x = eval(car_(cdr), env, gc);
            auto y = eval(car_(cdr_(cdr)), env, gc);
            return check(x == y);
        } else if (a->value == "COND") {
            auto branches = cdr;
            while (true) {
                auto p = eval(car_(car_(branches)), env, gc);
                if (p == gc.true_()) {
                    return eval(car_(cdr_(car_(branches))), env, gc);
                } else {
                    branches = cdr_(branches);
                }
            }
        } else if (a->value == "CAR") {
            return car_(eval(car_(cdr), env, gc));
        } else if (a->value == "CDR") {
            return cdr_(eval(car_(cdr), env, gc));
        } else if (a->value == "CONS") {
            auto x = eval(car_(cdr), env, gc);
            auto xs = eval(car_(cdr_(cdr)), env, gc);
            return gc.alloc_cons(x, xs);
        } else {
            auto v = env.at(a->value);
            auto evlis = [&env, &gc](ast::Expr *e) {
                std::vector<ast::Expr *> vec;
                while (e != gc.nil()) {
                    vec.push_back(eval(car_(e), env, gc));
                    e = cdr_(e);
                }
                ast::Expr *ret = gc.nil();
                for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                    ret = gc.alloc_cons(*it, ret);
                }
                return ret;
            };
            return eval(gc.alloc_cons(v, evlis(cdr)), env, gc);
        }
    }
    throw std::runtime_error("unimplemented for " + e->format());
}

#endif

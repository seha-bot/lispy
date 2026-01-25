#ifndef EVAL_HPP
#define EVAL_HPP

#include <stdexcept>

#include "alloc.hpp"
#include "ast.hpp"

using Env = std::unordered_map<std::string_view, ast::Expr *>;

ast::Expr *eval(ast::Expr const *e, Env& env, Alloc& alloc) {
    if (auto *a = dynamic_cast<ast::ExprAtom const *>(e)) {
        return env.at(a->value);
    }

    auto *car = dynamic_cast<ast::ExprCons const&>(*e).car;
    auto *cdr = dynamic_cast<ast::ExprCons const&>(*e).cdr;

    auto check = [&alloc](bool b) { return b ? alloc.true_() : alloc.false_(); };

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car)) {
        if (a->value == "QUOTE") {
            return car_(cdr);
        } else if (a->value == "ATOM") {
            auto expr = eval(car_(cdr), env, alloc);
            return check(is_atom(expr));
        } else if (a->value == "EQ") {
            auto x = eval(car_(cdr), env, alloc);
            auto y = eval(car_(cdr_(cdr)), env, alloc);
            return check(x == y);
        } else if (a->value == "COND") {
            auto branches = cdr;
            while (true) {
                auto p = eval(car_(car_(branches)), env, alloc);
                if (p == alloc.true_()) {
                    return eval(car_(cdr_(car_(branches))), env, alloc);
                } else {
                    branches = cdr_(branches);
                }
            }
        } else if (a->value == "CAR") {
            return car_(eval(car_(cdr), env, alloc));
        } else if (a->value == "CDR") {
            return cdr_(eval(car_(cdr), env, alloc));
        } else if (a->value == "CONS") {
            auto x = eval(car_(cdr), env, alloc);
            auto xs = eval(car_(cdr_(cdr)), env, alloc);
            return alloc.cons(x, xs);
        } else {
            auto v = env.at(a->value);
            auto evlis = [&env, &alloc](ast::Expr *e) {
                std::vector<ast::Expr *> vec;
                while (e != alloc.nil()) {
                    vec.push_back(eval(car_(e), env, alloc));
                    e = cdr_(e);
                }
                ast::Expr *ret = alloc.nil();
                for (auto it = vec.rbegin(); it != vec.rend(); ++it) {
                    ret = alloc.cons(*it, ret);
                }
                return ret;
            };
            return eval(alloc.cons(v, evlis(cdr)), env, alloc);
        }
    }
    throw std::runtime_error("unimplemented for " + e->format());
}

#endif

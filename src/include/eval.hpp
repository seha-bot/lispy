#ifndef EVAL_HPP
#define EVAL_HPP

#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "alloc.hpp"
#include "ast.hpp"

using Env = std::vector<std::pair<ast::ExprAtom *, ast::Expr *>>;

ast::Expr *env_get(ast::ExprAtom *a, Env const& genv, Env const& lenv) {
    for (auto it = lenv.rbegin(); it != lenv.rend(); ++it) {
        if (it->first == a) {
            return it->second;
        }
    }
    for (auto [k, v] : genv) {
        if (k == a) {
            return v;
        }
    }
    throw std::runtime_error("Substitution for " + std::string(a->value) + " not found.");
}

ast::Expr *eval_impl(Alloc& alloc, ast::Expr *e, Env& genv, Env& lenv) {
    // std::cout << "[DEBUG] " << e->format() << '\n';

    if (auto *a = dynamic_cast<ast::ExprAtom *>(e)) {
        return env_get(a, genv, lenv);
    }

    auto *car = dynamic_cast<ast::ExprCons&>(*e).car;
    auto *cdr = dynamic_cast<ast::ExprCons&>(*e).cdr;

    auto check = [&alloc](bool b) { return b ? alloc.true_() : alloc.false_(); };

    auto printenv = [&lenv] {
        return;
        std::cout << "NEW ENV: ";
        for (auto [k, v] : lenv) {
            std::cout << k->format() << " -> " << v->format() << " ;; ";
        }
        std::cout << '\n';
    };

    auto eval_ = [&alloc, &genv, &lenv](ast::Expr *e) { return eval_impl(alloc, e, genv, lenv); };

    auto f = e->format();

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car)) {
        if (a->value == "PRINT") {
            auto expr = eval_(car_(cdr));
            std::cout << expr->format();
            return expr;
        } else if (a->value == "EVAL") {
            return eval_(eval_(car_(cdr)));
        } else if (a->value == "QUOTE") {
            return car_(cdr);
        } else if (a->value == "ATOM") {
            auto expr = eval_(car_(cdr));
            return check(is_atom(expr));
        } else if (a->value == "EQ") {
            auto x = eval_(car_(cdr));
            auto y = eval_(car_(cdr_(cdr)));
            if (not is_atom(x) or not is_atom(y)) {
                throw std::runtime_error("EQ requires atoms.");
            }
            return check(x == y);
        } else if (a->value == "COND") {
            auto branches = cdr;
            while (true) {
                auto p = eval_(car_(car_(branches)));
                if (p == alloc.true_()) {
                    return eval_(car_(cdr_(car_(branches))));
                } else {
                    branches = cdr_(branches);
                }
            }
        } else if (a->value == "CAR") {
            return car_(eval_(car_(cdr)));
        } else if (a->value == "CDR") {
            return cdr_(eval_(car_(cdr)));
        } else if (a->value == "CONS") {
            auto x = eval_(car_(cdr));
            auto xs = eval_(car_(cdr_(cdr)));
            return alloc.cons(x, xs);
        } else if (a->value == "LABEL") {
            auto identifier = &dynamic_cast<ast::ExprAtom&>(*eval_(car_(cdr)));
            auto *body = car_(cdr_(cdr));
            lenv.emplace_back(identifier, body);
            auto *r = eval_(body);
            lenv.pop_back();
            return r;
        } else if (a->value == "DEFINE") {
            // auto identifier = &dynamic_cast<ast::ExprAtom&>(*eval_(car_(cdr)));
            auto identifier = &dynamic_cast<ast::ExprAtom&>(*car_(cdr));
            auto *body = car_(cdr_(cdr));
            body->lock();
            genv.emplace_back(identifier, body);
            return identifier;
        } else if (a->value == "LAMBDA") {
            return e;
        } else if (a->value == "MACRO") {
            return e;
        } else {
            return eval_(alloc.cons(env_get(a, genv, lenv), cdr));
        }
    }

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car_(car)); a and a->value == "LAMBDA") {
        auto evlis_vec = [&alloc, &eval_](ast::Expr *e) {
            std::vector<ast::Expr *> vec;
            while (e != alloc.nil()) {
                vec.push_back(eval_(car_(e)));
                e = cdr_(e);
            }
            return vec;
        };
        std::size_t param_cnt = 0;
        {
            // TODO: UB if #args < #params
            std::vector<ast::Expr *> args = evlis_vec(cdr);
            // ast::Expr *params = eval_(car_(cdr_(car)));
            ast::Expr *params = car_(cdr_(car));
            while (params != alloc.nil()) {
                auto *p = &dynamic_cast<ast::ExprAtom&>(*car_(params));
                lenv.emplace_back(p, args[param_cnt++]);
                params = cdr_(params);
            }
        }
        printenv();
        auto *r = eval_(car_(cdr_(cdr_(car))));
        lenv.resize(lenv.size() - param_cnt);
        printenv();
        return r;
    }

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car_(car)); a and a->value == "MACRO") {
        lenv.emplace_back(&dynamic_cast<ast::ExprAtom&>(*car_(cdr_(car))), cdr);
        printenv();
        auto *r = eval_impl(alloc, car_(cdr_(cdr_(car))), genv, lenv);
        lenv.pop_back();
        printenv();
        return r;
    }

    throw std::runtime_error("unimplemented for " + e->format());
}

ast::Expr *eval(Alloc& alloc, ast::Expr *e, Env& genv) {
    Env lenv;
    auto *r = eval_impl(alloc, e, genv, lenv);
    if (not lenv.empty()) {
        throw std::logic_error("The evaluation stack contains garbage. Something went really wrong.");
    }
    return r;
}

#endif

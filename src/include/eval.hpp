#ifndef EVAL_HPP
#define EVAL_HPP

#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

#include "alloc.hpp"
#include "ast.hpp"

using Env = std::vector<std::pair<ast::ExprAtom *, ast::Expr *>>;

ast::Expr *env_get(Env const& env, ast::ExprAtom *a) {
    for (auto it = env.rbegin(); it != env.rend(); ++it) {
        if (it->first == a) {
            return it->second;
        }
    }
    throw std::runtime_error("Substitution for " + std::string(a->value) + " not found.");
}

ast::Expr *eval(ast::Expr *e, Env& env, Alloc& alloc) {
    // std::cout << "[DEBUG] " << e->format() << '\n';

    if (auto *a = dynamic_cast<ast::ExprAtom *>(e)) {
        return env_get(env, a);
    }

    auto *car = dynamic_cast<ast::ExprCons&>(*e).car;
    auto *cdr = dynamic_cast<ast::ExprCons&>(*e).cdr;

    auto check = [&alloc](bool b) { return b ? alloc.true_() : alloc.false_(); };

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car)) {
        if (a->value == "PRINT") {
            auto expr = eval(car_(cdr), env, alloc);
            std::cout << expr->format();
            return expr;
        } else if (a->value == "QUOTE") {
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
        } else if (a->value == "LABEL") {
            auto *identifier = &dynamic_cast<ast::ExprAtom&>(*car_(cdr));
            auto *body = car_(cdr_(cdr));
            body->lock();
            env.emplace_back(identifier, body);
            return eval(body, env, alloc);
        } else if (a->value == "LAMBDA") {
            return e;
        } else {
            auto *v = env_get(env, a);
            return eval(alloc.cons(v, cdr), env, alloc);
        }
    }

    if (auto *a = dynamic_cast<ast::ExprAtom *>(car_(car)); a and a->value == "LAMBDA") {
        auto evlis_vec = [&env, &alloc](ast::Expr *e) {
            std::vector<ast::Expr *> vec;
            while (e != alloc.nil()) {
                vec.push_back(eval(car_(e), env, alloc));
                e = cdr_(e);
            }
            return vec;
        };
        std::size_t param_cnt = 0;
        {
            // TODO: UB if #args < #params
            std::vector<ast::Expr *> args = evlis_vec(cdr);
            ast::Expr *params = car_(cdr_(car));
            while (params != alloc.nil()) {
                auto *p = &dynamic_cast<ast::ExprAtom&>(*car_(params));
                env.emplace_back(p, args[param_cnt++]);
                params = cdr_(params);
            }
        }
        auto *r = eval(car_(cdr_(cdr_(car))), env, alloc);
        env.resize(env.size() - param_cnt);
        return r;
    }

    throw std::runtime_error("unimplemented for " + e->format());
}

#endif

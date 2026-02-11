#include "explorer.hpp"

#include <cstddef>
#include <memory>
#include <span>
#include <string>
#include <vector>

[[noreturn]] static void todo() { throw std::runtime_error("unimplemented"); }

struct DefEnv {
    struct Checkpoint {
        std::size_t unbound;
    };
    Checkpoint checkpoint() const { return Checkpoint{m_unbound_names.size()}; }
    void mark_unbound(ast::Atom& atom) { m_unbound_names.push_back(&atom); }

    void rewrite_from(Checkpoint checkpoint, Closure const& closure) {
        for (auto i = checkpoint.unbound; i < m_unbound_names.size();) {
            if (closure.is_defined(m_unbound_names[i]->value)) {
                m_unbound_names.erase(m_unbound_names.begin() + static_cast<std::ptrdiff_t>(i));
            } else {
                ++i;
            }
        }
    }

    void for_each_unbound_from(Checkpoint checkpoint, auto fn) {
        for (auto i = checkpoint.unbound; i < m_unbound_names.size(); ++i) {
            fn(*m_unbound_names[i]);
        }
    }

    Closure& register_closure(Closure closure) {
        m_closures.push_back(std::make_unique<Closure>(std::move(closure)));
        return *m_closures.back();
    }

    bool has_unbound_names() const { return not m_unbound_names.empty(); }
    Program to_program() && { return Program{std::move(m_closures)}; }

    std::vector<std::unique_ptr<Closure>> const& closures() const { return m_closures; }

private:
    std::vector<ast::Atom *> m_unbound_names;
    std::vector<std::unique_ptr<Closure>> m_closures;
};

static std::expected<std::unordered_map<std::string, ast::Expr *>, StaticError> discover_definitions(
    DefEnv& env, std::span<ast::ExprPtr const> body) {
    std::unordered_map<std::string, ast::Expr *> res;
    for (auto& expr : body) {
        if (expr->type() != ast::ExprType::list) {
            continue;
        }
        auto& list = static_cast<ast::List&>(*expr);
        if (list.elements.size() < 2 or list.elements[0]->type() != ast::ExprType::atom) {
            continue;
        }
        auto& callee = static_cast<ast::Atom&>(*list.elements[0]);
        if (callee.value != "define") {
            continue;
        }
        switch (list.elements[1]->type()) {
            case ast::ExprType::atom: {
                auto& name = static_cast<ast::Atom&>(*list.elements[1]);
                if (not res.emplace(name.value, &list).second) {
                    todo();
                }
                break;
            }
            case ast::ExprType::list: {
                auto& name_and_params = static_cast<ast::List&>(*list.elements[1]);
                if (name_and_params.elements.empty()) {
                    break;
                }
                auto& name = static_cast<ast::Atom&>(*name_and_params.elements[0]);
                if (not res.emplace(name.value, &list).second) {
                    todo();
                }
                break;
            }
            default:
                break;
        }
    }
    return res;
}

static std::expected<void, StaticError> do_expr(DefEnv& env, Closure& closure, ast::Expr& expr);

static std::expected<void, StaticError> do_lambda(DefEnv& env, Closure *parent, ast::List *source,
                                                  std::span<ast::ExprPtr> params, std::span<ast::ExprPtr const> body) {
    std::unordered_map<std::string, std::size_t> parameters;
    for (auto& param : params) {
        if (param->type() != ast::ExprType::atom) {
            todo();
        }
        auto& param_name = static_cast<ast::Atom&>(*param).value;
        if (not parameters.emplace(param_name, parameters.size()).second) {
            todo();
        }
    }

    auto definitions = discover_definitions(env, body);
    if (not definitions) {
        return std::unexpected(definitions.error());
    }

    auto& clsr = env.register_closure(Closure(std::move(parameters), *std::move(definitions), parent, source));

    auto checkpoint = env.checkpoint();
    for (auto& expr : body) {
        auto res = do_expr(env, clsr, *expr);
        if (not res) {
            return std::unexpected(res.error());
        }
    }

    env.rewrite_from(checkpoint, clsr);
    env.for_each_unbound_from(checkpoint, [&](ast::Atom& atom) { clsr.add_capture(atom.value); });
    return {};
}

static std::expected<void, StaticError> do_define(DefEnv& env, Closure& closure, ast::List& list) {
    if (list.elements.size() < 3) {
        todo();
    }

    switch (list.elements[1]->type()) {
        case ast::ExprType::atom: {
            if (list.elements.size() != 3) {
                todo();
            }
            return do_expr(env, closure, *list.elements[2]);
        }
        case ast::ExprType::list: {
            auto& name_and_params = static_cast<ast::List&>(*list.elements[1]);
            if (name_and_params.elements.empty()) {
                todo();
            }
            auto& name = static_cast<ast::Atom&>(*name_and_params.elements[0]).value;
            auto params = std::span(name_and_params.elements).subspan(1);
            auto body = std::span(list.elements).subspan(2);
            return do_lambda(env, &closure, &list, params, body);
        }
        case ast::ExprType::number:
            todo();
        case ast::ExprType::string:
            todo();
    }
}

static std::expected<void, StaticError> do_expr(DefEnv& env, Closure& closure, ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            if (not closure.is_static(env.closures(), atom.value)) {
                env.mark_unbound(atom);
            }
            return {};
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr);
            if (list.elements.empty()) {
                return {};
            }
            switch (list.elements[0]->type()) {
                case ast::ExprType::atom: {
                    auto& atom = static_cast<ast::Atom&>(*list.elements[0]);
                    if (atom.value == "define") {
                        return do_define(env, closure, list);
                    } else if (atom.value == "lambda") {
                        if (list.elements.size() < 3) {
                            todo();
                        }
                        if (list.elements[1]->type() != ast::ExprType::list) {
                            todo();
                        }
                        auto& params = static_cast<ast::List&>(*list.elements[1]).elements;
                        auto body = std::span(list.elements).subspan(2);
                        return do_lambda(env, &closure, &list, params, body);
                    } else if (atom.value == "quote") {
                        return {};
                    } else if (atom.value == "if") {
                        if (list.elements.size() != 4) {
                            todo();
                        }
                        for (std::size_t i = 1; i < list.elements.size(); ++i) {
                            auto r = do_expr(env, closure, *list.elements[i]);
                            if (not r) {
                                return r;
                            }
                        }
                        return {};
                    }
                    [[fallthrough]];
                }
                case ast::ExprType::list:
                    for (auto& call_expr : list.elements) {
                        auto res = do_expr(env, closure, *call_expr);
                        if (not res) {
                            return std::unexpected(res.error());
                        }
                    }
                    return {};
                case ast::ExprType::number:
                    todo();
                case ast::ExprType::string:
                    todo();
            }
        }
        case ast::ExprType::number:
        case ast::ExprType::string:
            return {};
    }
}

std::expected<Program, std::vector<StaticError>> do_program(std::vector<ast::ExprPtr> const& exprs) {
    DefEnv env;
    std::vector<StaticError> errors;
    auto program = do_lambda(env, nullptr, nullptr, {}, exprs);
    if (not program) {
        errors.push_back(program.error());
    }

    if (env.has_unbound_names()) {
        env.for_each_unbound_from(
            {}, [&](ast::Atom& atom) { errors.push_back(StaticError{"Unbound name: \"" + atom.value + '"', atom}); });
    }

    if (not errors.empty()) {
        return std::unexpected(std::move(errors));
    }

    return std::move(env).to_program();
}

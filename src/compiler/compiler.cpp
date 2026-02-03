#include "compiler.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <unordered_set>
#include <utility>

#include "ast.hpp"

[[noreturn]] static void todo() { throw std::runtime_error("unimplemented"); }

static CompilationResult compile_quote(std::vector<Line>& code, ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            code.push_back(Instruction{Mnemonic::push, AtomOperand{atom.value}});
            return CompilationResult::ok;
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr).elements;
            for (auto& e : list) {
                auto arg = compile_quote(code, *e);
                if (arg != CompilationResult::ok) {
                    return arg;
                }
            }
            code.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
            for (auto& _ : list) {
                code.push_back(Instruction{Mnemonic::cons});
            }
            return CompilationResult::ok;
        }
        case ast::ExprType::number: {
            auto& number = static_cast<ast::Number&>(expr);
            code.push_back(Instruction{Mnemonic::push, LiteralOperand{number.value}});
            return CompilationResult::ok;
        }
        case ast::ExprType::string:
            todo();
    }
}

struct Env {
    std::unordered_set<std::string> locals;
    int lambda_cnt;
    int label_cnt;
    int stack_depth;
};

static CompilationResult compile_expr(Env& env, std::vector<Entity>& entities, Entity& entity, ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            if (env.locals.contains(atom.value)) {
                if (not entity.parameters.contains(atom.value)) {
                    // TODO: check duplicates
                    entity.captures.push_back(atom.value);
                }
                entity.code.push_back(
                    Instruction{Mnemonic::push,
                                LiteralOperand{env.stack_depth + entity.stack_index(atom.value), true, atom.value}});
            } else {
                entity.code.push_back(Instruction{Mnemonic::push, LabelOperand{atom.value}});
            }
            return CompilationResult::ok;
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr);
            if (list.elements.empty()) {
                entity.code.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
                return CompilationResult::ok;
            }

            switch (list.elements[0]->type()) {
                case ast::ExprType::atom: {
                    auto& callee = static_cast<ast::Atom&>(*list.elements[0]).value;

                    auto compile_with_args = [&](Mnemonic m, std::size_t n) {
                        // TODO: arity check should be an ICE at this point.
                        // This needs to be moved to a static check step at the AST level.
                        if (list.elements.size() != n + 1) {
                            return CompilationResult::incorrect_arity;
                        }
                        auto old_depth = env.stack_depth;
                        for (std::size_t i = 0; i < n; i++) {
                            auto arg = compile_expr(env, entities, entity, *list.elements[i + 1]);
                            if (arg != CompilationResult::ok) {
                                return arg;
                            }
                            env.stack_depth += 1;
                        }
                        env.stack_depth = old_depth;
                        entity.code.push_back(Instruction{m});
                        return CompilationResult::ok;
                    };

                    if (callee == "eq") {
                        return compile_with_args(Mnemonic::eq, 2);
                    } else if (callee == "cons") {
                        return compile_with_args(Mnemonic::cons, 2);
                    } else if (callee == "car") {
                        return compile_with_args(Mnemonic::car, 1);
                    } else if (callee == "cdr") {
                        return compile_with_args(Mnemonic::cdr, 1);
                    } else if (callee == "if") {
                        if (list.elements.size() != 4) {
                            return CompilationResult::incorrect_arity;
                        }

                        if (auto cond = compile_expr(env, entities, entity, *list.elements[1]);
                            cond != CompilationResult::ok) {
                            return cond;
                        }
                        auto label_false = ".L" + std::to_string(env.label_cnt++);
                        entity.code.push_back(Instruction{Mnemonic::jf, LabelOperand{label_false}});
                        if (auto then = compile_expr(env, entities, entity, *list.elements[2]);
                            then != CompilationResult::ok) {
                            return then;
                        }
                        auto label_end = ".L" + std::to_string(env.label_cnt++);
                        entity.code.push_back(Instruction{Mnemonic::jmp, LabelOperand{label_end}});
                        entity.code.push_back(Label{std::move(label_false)});
                        if (auto else_ = compile_expr(env, entities, entity, *list.elements[3]);
                            else_ != CompilationResult::ok) {
                            return else_;
                        }
                        entity.code.push_back(Label{label_end});
                        return CompilationResult::ok;
                    } else if (callee == "print") {
                        return compile_with_args(Mnemonic::print, 1);
                    } else if (callee == "quote") {
                        if (list.elements.size() != 2) {
                            return CompilationResult::incorrect_arity;
                        }
                        return compile_quote(entity.code, *list.elements[1]);
                    } else if (callee == "lambda") {
                        if (list.elements.size() < 3) {
                            return CompilationResult::incorrect_arity;
                        }

                        auto& params_expr = *list.elements[1];
                        if (params_expr.type() != ast::ExprType::list) {
                            return CompilationResult::parameters_not_well_formed;
                        }
                        auto& params = static_cast<ast::List&>(params_expr);
                        for (auto& param : params.elements) {
                            if (not param->is_atom()) {
                                return CompilationResult::parameters_not_well_formed;
                            }
                        }

                        auto unsafe_atom_name = [](ast::Expr& expr) {
                            return static_cast<ast::Atom const&>(expr).value;
                        };

                        Entity lambda{entity.name + "lam" + std::to_string(env.lambda_cnt++), true};
                        for (auto& param : params.elements) {
                            // TODO: check duplicates on both
                            env.locals.insert(unsafe_atom_name(*param));
                            lambda.parameters.emplace(unsafe_atom_name(*param), lambda.parameters.size());
                        }

                        for (std::size_t i = 2; i < list.elements.size(); ++i) {
                            auto res = compile_expr(env, entities, lambda, *list.elements[i]);
                            if (res != CompilationResult::ok) {
                                return res;
                            }
                            if (i != list.elements.size() - 1) {
                                lambda.code.push_back(Instruction{Mnemonic::pop, LiteralOperand{1}});
                            }
                        }

                        for (auto& param : params.elements) {
                            env.locals.erase(unsafe_atom_name(*param));
                        }

                        if (params.elements.size() + lambda.captures.size() == 0) {
                            lambda.code.push_back(Instruction{Mnemonic::ret, LiteralOperand{0}});
                        } else {
                            auto n_elements =
                                static_cast<std::int64_t>(params.elements.size() + lambda.captures.size());
                            lambda.code.push_back(
                                Instruction{Mnemonic::set, LiteralOperand{n_elements}, LiteralOperand{0, true}});
                            lambda.code.push_back(Instruction{Mnemonic::ret, LiteralOperand{n_elements}});
                        }

                        // TODO: maybe this should go into the optimizer step, but idk.
                        if (lambda.captures.empty()) {
                            entity.code.push_back(Instruction{Mnemonic::push, LabelOperand{lambda.name}});
                        } else {
                            entity.code.push_back(Instruction{Mnemonic::closure, LabelOperand{lambda.name}});
                            for (auto& capture : lambda.captures) {
                                if (not entity.parameters.contains(capture)) {
                                    // TODO: check duplicates
                                    entity.captures.push_back(capture);
                                }
                                entity.code.push_back(Instruction{
                                    Mnemonic::capture,
                                    LiteralOperand{env.stack_depth + entity.stack_index(capture) + 1, true, capture}});
                            }
                        }

                        entities.push_back(std::move(lambda));

                        return CompilationResult::ok;
                    } else {
                        auto old_depth = env.stack_depth;
                        for (std::size_t i = 1; i < list.elements.size(); i++) {
                            auto arg = compile_expr(env, entities, entity, *list.elements[i]);
                            if (arg != CompilationResult::ok) {
                                return arg;
                            }
                            env.stack_depth += 1;
                        }

                        bool is_local = entity.parameters.contains(callee);
                        if (is_local) {
                            if (not entity.parameters.contains(callee)) {
                                // TODO: check duplicates
                                entity.captures.push_back(callee);
                            }
                            entity.code.push_back(Instruction{
                                Mnemonic::indcall,
                                LiteralOperand{env.stack_depth + entity.stack_index(callee), true, callee}});
                        } else {
                            entity.code.push_back(Instruction{Mnemonic::call, LabelOperand{callee}});
                        }

                        env.stack_depth = old_depth;
                        return CompilationResult::ok;
                    }
                }
                case ast::ExprType::list: {
                    auto old_depth = env.stack_depth;
                    for (std::size_t i = 1; i < list.elements.size(); i++) {
                        auto arg = compile_expr(env, entities, entity, *list.elements[i]);
                        if (arg != CompilationResult::ok) {
                            return arg;
                        }
                        env.stack_depth += 1;
                    }
                    auto callee = compile_expr(env, entities, entity, *list.elements[0]);
                    if (callee != CompilationResult::ok) {
                        return callee;
                    }
                    env.stack_depth = old_depth;

                    entity.code.push_back(Instruction{Mnemonic::indcall, LiteralOperand{0, true}});
                    return CompilationResult::ok;
                }
                case ast::ExprType::number:
                    todo();
                case ast::ExprType::string:
                    todo();
            }
        }
        case ast::ExprType::number: {
            auto& number = static_cast<ast::Number&>(expr);
            entity.code.push_back(Instruction{Mnemonic::push, LiteralOperand{number.value}});
            return CompilationResult::ok;
        }
        case ast::ExprType::string: {
            auto& string = static_cast<ast::String&>(expr).value;
            entity.code.push_back(Instruction{Mnemonic::mkstr});
            for (char c : string) {
                entity.code.push_back(Instruction{Mnemonic::strpush, CharOperand{c}});
            }
            return CompilationResult::ok;
        }
    }
}

static CompilationResult compile_define(std::vector<Entity>& entities, ast::List& list) {
    if (list.elements.size() < 3) {
        return CompilationResult::incorrect_arity;
    }
    auto& params_expr = *list.elements[1];

    switch (params_expr.type()) {
        case ast::ExprType::atom:
            todo();
        case ast::ExprType::list: {
            auto& params = static_cast<ast::List&>(params_expr);
            if (params.elements.empty()) {
                return CompilationResult::define_missing_name;
            }
            for (auto& param : params.elements) {
                if (not param->is_atom()) {
                    return CompilationResult::parameters_not_well_formed;
                }
            }

            auto unsafe_atom_name = [](ast::Expr& expr) { return static_cast<ast::Atom const&>(expr).value; };

            Env env;

            Entity entity{unsafe_atom_name(*params.elements[0]), false};
            for (std::size_t i = 1; i < params.elements.size(); ++i) {
                // TODO: check duplicates on both
                env.locals.insert(unsafe_atom_name(*params.elements[i]));
                entity.parameters.emplace(unsafe_atom_name(*params.elements[i]), entity.parameters.size());
            }

            for (std::size_t i = 2; i < list.elements.size(); ++i) {
                auto res = compile_expr(env, entities, entity, *list.elements[i]);
                if (res != CompilationResult::ok) {
                    return res;
                }
                if (i != list.elements.size() - 1) {
                    entity.code.push_back(Instruction{Mnemonic::pop, LiteralOperand{1}});
                }
            }
            if (params.elements.size() == 1) {
                entity.code.push_back(Instruction{Mnemonic::ret, LiteralOperand{0}});
            } else {
                auto n_params = static_cast<std::int64_t>(params.elements.size() - 1);
                entity.code.push_back(Instruction{Mnemonic::set, LiteralOperand{n_params}, LiteralOperand{0, true}});
                entity.code.push_back(Instruction{Mnemonic::ret, LiteralOperand{n_params}});
            }

            // This will be removed completely once Entity is modeled properly
            if (not entity.captures.empty()) {
                throw "lol";
            }

            entities.push_back(std::move(entity));
            return CompilationResult::ok;
        }
        case ast::ExprType::number:
            todo();
        case ast::ExprType::string:
            todo();
    }
}

CompilationResult compile(std::vector<Entity>& entities, ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom:
            todo();
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr);

            if (list.elements.empty()) {
                return CompilationResult::ok;
            }

            auto& function_expr = *list.elements.front();
            if (not function_expr.is_atom()) {
                todo();
            }
            auto& function = static_cast<ast::Atom&>(function_expr);

            if (function.value == "define") {
                return compile_define(entities, list);
            }

            todo();
        }
        case ast::ExprType::number:
            todo();
        case ast::ExprType::string:
            todo();
    }
}

static std::string to_string(AtomOperand const& o) { return '\'' + o.name; }

static std::string to_string(LiteralOperand const& o) {
    if (o.is_stack) {
        auto r = '[' + std::to_string(o.value) + ']';
        if (not o.parameter_name.empty()) {
            r += " ; " + o.parameter_name;
        }
        return r;
    }
    return std::to_string(o.value);
}

static std::string to_string(CharOperand const& o) { return '\'' + std::string{o.value} + '\''; }

static std::string to_string(LabelOperand const& o) { return o.name; }

static std::string to_string(Operand const& o) {
    return std::visit([](auto&& x) { return to_string(x); }, o);
}

static std::string format_line(Line const& line) {
    struct Visitor {
        auto operator()(Instruction i) {
            auto r = '\t' + to_string(i.mnemonic);
            if (i.o1.index() != 0) {
                r += ' ' + to_string(i.o1);
            }
            if (i.o2.index() != 0) {
                r += ' ' + to_string(i.o2);
            }
            return r;
        }
        auto operator()(Label label) { return label.name + ':'; }
    };
    return std::visit(Visitor{}, line);
}

std::string format_entity(Entity const& entity) {
    auto r = entity.name;
    r += ':';
    if (entity.parameters.size() + entity.captures.size() != 0) {
        r += " ;";
    }

    if (not entity.parameters.empty()) {
        std::vector<std::string> ordered(entity.parameters.size());
        for (auto& [name, i] : entity.parameters) {
            ordered[i] = name;
        }
        for (auto& name : ordered) {
            r += ' ' + name;
        }
    }
    if (not entity.captures.empty()) {
        r += " {";
        for (char sep[] = "\0"; auto& name : entity.captures) {
            r += sep + name;
            sep[0] = ' ';
        }
        r += '}';
    }
    r += '\n';
    for (auto& line : entity.code) {
        r += format_line(line) + '\n';
    }
    return r;
}

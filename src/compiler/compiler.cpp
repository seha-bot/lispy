#include "compiler.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "ast.hpp"

namespace compiler {

[[noreturn]] static void todo() { throw std::runtime_error("unimplemented"); }

static void compile_quote(std::vector<Line>& lines, ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            lines.push_back(Instruction{Mnemonic::push, AtomOperand{atom.value}});
            break;
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr).elements;
            for (auto& e : list) {
                compile_quote(lines, *e);
            }
            lines.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
            for (auto& _ : list) {
                lines.push_back(Instruction{Mnemonic::cons, std::nullopt});
            }
            break;
        }
        case ast::ExprType::number: {
            auto& number = static_cast<ast::Number&>(expr);
            lines.push_back(Instruction{Mnemonic::push, LiteralOperand{number.value}});
            break;
        }
        case ast::ExprType::string:
            todo();
    }
}

struct Env {
    std::unordered_map<ast::Expr *, std::pair<std::string, Closure const *>> lambdas;
    std::size_t label_cnt = 0;
    std::size_t stack_depth = 0;
};

static std::expected<void, StaticError> compile_expr(std::vector<Line>& lines, Env& env, Closure const& closure,
                                                     ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            if (closure.is_local(atom.value)) {
                lines.push_back(
                    Instruction{Mnemonic::push, StackOperand{env.stack_depth + closure.index_header(atom.value)}});
            } else {
                lines.push_back(Instruction{Mnemonic::push, LabelOperand{atom.value}});
            }
            return {};
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr);
            if (list.elements.empty()) {
                lines.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
                return {};
            }

            switch (list.elements[0]->type()) {
                case ast::ExprType::atom: {
                    auto& callee = static_cast<ast::Atom&>(*list.elements[0]).value;

                    if (callee == "if") {
                        if (list.elements.size() != 4) {
                            // return CompilationResult::incorrect_arity;
                            todo();
                        }

                        if (auto cond = compile_expr(lines, env, closure, *list.elements[1]); not cond) {
                            return cond;
                        }
                        auto label_false = env.label_cnt++;
                        lines.push_back(Instruction{Mnemonic::jf, LocalLabelOperand{label_false}});
                        if (auto then = compile_expr(lines, env, closure, *list.elements[2]); not then) {
                            return then;
                        }
                        auto label_end = env.label_cnt++;
                        lines.push_back(Instruction{Mnemonic::jmp, LocalLabelOperand{label_end}});
                        lines.push_back(LocalLabel{label_false});
                        if (auto else_ = compile_expr(lines, env, closure, *list.elements[3]); not else_) {
                            return else_;
                        }
                        lines.push_back(LocalLabel{label_end});
                        return {};
                    } else if (callee == "quote") {
                        if (list.elements.size() != 2) {
                            // return CompilationResult::incorrect_arity;
                            todo();
                        }
                        compile_quote(lines, *list.elements[1]);
                        return {};
                    } else if (callee == "lambda") {
                        auto& [name, subclosure] = env.lambdas.at(&list);
                        auto captures = subclosure->captures();

                        // TODO: maybe this should go into the optimizer step, but idk.
                        if (captures.empty()) {
                            lines.push_back(Instruction{Mnemonic::push, LabelOperand{name}});
                        } else {
                            lines.push_back(Instruction{Mnemonic::closure, LabelOperand{name}});
                            for (auto& capture : captures) {
                                lines.push_back(
                                    Instruction{Mnemonic::capture,
                                                StackOperand{env.stack_depth + 1 + closure.index_header(capture)}});
                            }
                        }
                        return {};
                    } else if (callee == "define") {
                        lines.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
                        return {};
                    } else {
                        auto old_depth = env.stack_depth;
                        for (std::size_t i = 1; i < list.elements.size(); ++i) {
                            auto res = compile_expr(lines, env, closure, *list.elements[i]);
                            if (not res) {
                                return std::unexpected(res.error());
                            }
                            env.stack_depth += 1;
                        }

                        if (closure.is_local(callee)) {
                            lines.push_back(Instruction{Mnemonic::indcall,
                                                        StackOperand{env.stack_depth + closure.index_header(callee)}});
                        } else {
                            lines.push_back(Instruction{Mnemonic::call, LabelOperand{callee}});
                        }
                        env.stack_depth = old_depth;

                        return {};
                    }
                }
                case ast::ExprType::list: {
                    auto old_depth = env.stack_depth;
                    for (std::size_t i = 1; i < list.elements.size(); ++i) {
                        auto res = compile_expr(lines, env, closure, *list.elements[i]);
                        if (not res) {
                            return std::unexpected(res.error());
                        }
                        env.stack_depth += 1;
                    }
                    auto res = compile_expr(lines, env, closure, *list.elements[0]);
                    if (not res) {
                        return std::unexpected(res.error());
                    }
                    env.stack_depth = old_depth;

                    lines.push_back(Instruction{Mnemonic::indcall, StackOperand{0}});
                    return {};
                }
                case ast::ExprType::number:
                    todo();
                case ast::ExprType::string:
                    todo();
            }
        }
        case ast::ExprType::number: {
            auto& number = static_cast<ast::Number&>(expr);
            lines.push_back(Instruction{Mnemonic::push, LiteralOperand{number.value}});
            return {};
        }
        case ast::ExprType::string:
            todo();
    }
}

static std::expected<void, StaticError> compile_closure(std::vector<Line>& lines, Env& env, Closure const& closure) {
    auto& list = closure.source();
    for (std::size_t i = 2; i < list.elements.size(); ++i) {
        auto res = compile_expr(lines, env, closure, *list.elements[i]);
        if (not res) {
            return std::unexpected(res.error());
        }
        lines.push_back(Instruction{Mnemonic::pop});
    }
    lines.pop_back();

    if (closure.header_size() == 0) {
        lines.push_back(Instruction{Mnemonic::ret, LiteralOperand{0}});
    } else {
        auto n_elements = static_cast<std::int64_t>(closure.header_size());
        lines.push_back(Instruction{Mnemonic::set, LiteralOperand{n_elements}});
        lines.push_back(Instruction{Mnemonic::ret, LiteralOperand{n_elements - 1}});
    }
    return {};
}

std::expected<Code, std::vector<StaticError>> compile_program(Program const& program) {
    Env env;
    int cnt = 0;
    for (auto& closure : program.closures) {
        if (not closure->parent()) {
            continue;
        }

        std::string name = [&closure, &cnt] {
            if (auto def_name = closure->parent()->name_for(*closure)) {
                return *def_name;
            } else {
                return "lam" + std::to_string(cnt++);
            }
        }();
        env.lambdas.insert({&closure->source(), {name, closure.get()}});
    }

    Code code;
    std::vector<StaticError> errors;
    for (auto& closure : program.closures) {
        if (not closure->parent()) {
            continue;
        }

        env.label_cnt = 0;
        env.stack_depth = 0;
        code.lines.push_back(Label{env.lambdas.at(&closure->source()).first});
        auto res = compile_closure(code.lines, env, *closure);
        if (not res) {
            errors.push_back(res.error());
        }
    }

    if (not errors.empty()) {
        return std::unexpected(std::move(errors));
    }
    return code;
}

}  // namespace compiler

#include "compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "explorer.hpp"
#include "mnemonic.hpp"

namespace compiler {

namespace {

[[noreturn]] void todo() { throw std::runtime_error("unimplemented"); }

void compile_quote(std::vector<Line>& lines, ast::Expr& expr) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            lines.push_back(Instruction{Mnemonic::push, AtomOperand{atom.name()}});
            break;
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr).elements();
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
            lines.push_back(Instruction{Mnemonic::push, LiteralOperand{number.value()}});
            break;
        }
        case ast::ExprType::string:
            todo();
    }
}

struct Env {
    std::unordered_map<ast::Expr *, std::pair<std::string, Closure const *>> lambdas;
    std::unordered_set<ast::Expr *> lambdas_to_skip;
    std::size_t label_cnt = 0;
    std::size_t stack_depth = 0;
};

std::expected<void, StaticError> compile_expr(std::vector<Line>& lines, Env& env, Closure const& closure,
                                              ast::Expr& expr, bool is_root) {
    switch (expr.type()) {
        case ast::ExprType::atom: {
            auto& atom = static_cast<ast::Atom&>(expr);
            if (closure.is_local(atom.name())) {
                lines.push_back(
                    Instruction{Mnemonic::push, StackOperand{env.stack_depth + closure.index_header(atom.name())}});
            } else {
                lines.push_back(Instruction{Mnemonic::push, LabelOperand{atom.name()}});
            }
            return {};
        }
        case ast::ExprType::list: {
            auto& list = static_cast<ast::List&>(expr);
            if (list.empty()) {
                lines.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
                return {};
            }

            switch (list[0].type()) {
                case ast::ExprType::atom: {
                    auto& callee = static_cast<ast::Atom&>(list[0]).name();

                    if (callee == "if") {
                        if (list.size() != 4) {
                            // return CompilationResult::incorrect_arity;
                            todo();
                        }

                        if (auto cond = compile_expr(lines, env, closure, list[1], false); not cond) {
                            return cond;
                        }
                        auto label_false = env.label_cnt++;
                        lines.push_back(Instruction{Mnemonic::jf, LocalLabelOperand{label_false}});
                        if (auto then = compile_expr(lines, env, closure, list[2], is_root); not then) {
                            return then;
                        }
                        auto label_end = env.label_cnt++;
                        lines.push_back(Instruction{Mnemonic::jmp, LocalLabelOperand{label_end}});
                        lines.push_back(LocalLabel{label_false});
                        if (auto else_ = compile_expr(lines, env, closure, list[3], is_root); not else_) {
                            return else_;
                        }
                        lines.push_back(LocalLabel{label_end});
                        return {};
                    } else if (callee == "quote") {
                        if (list.size() != 2) {
                            // return CompilationResult::incorrect_arity;
                            todo();
                        }
                        compile_quote(lines, list[1]);
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
                        bool const tco_eligible = is_root and env.lambdas.at(&closure.source()).first == callee;
                        auto old_depth = env.stack_depth;
                        for (std::size_t i = 1; i < list.size(); ++i) {
                            auto arg = compile_expr(lines, env, closure, list[i], false);
                            if (not arg) {
                                return std::unexpected(arg.error());
                            }
                            if (tco_eligible) {
                                lines.push_back(Instruction{Mnemonic::set, LiteralOperand{static_cast<std::int64_t>(
                                                                               closure.header_size() - i + 1)}});
                            } else {
                                env.stack_depth += 1;
                            }
                        }

                        if (closure.is_local(callee)) {
                            lines.push_back(Instruction{Mnemonic::indcall,
                                                        StackOperand{env.stack_depth + closure.index_header(callee)}});
                        } else {
                            if (tco_eligible) {
                                lines.push_back(Instruction{Mnemonic::jmp, LabelOperand{callee}});
                            } else {
                                lines.push_back(Instruction{Mnemonic::call, LabelOperand{callee}});
                            }
                        }
                        env.stack_depth = old_depth;

                        return {};
                    }
                }
                case ast::ExprType::list: {
                    // IILE inlining
                    if (list.size() == 1) {
                        if (auto it = env.lambdas.find(&list[0]); it != env.lambdas.end()) {
                            auto& lambda = static_cast<ast::List&>(list[0]);
                            for (std::size_t i = 2; i < lambda.size(); ++i) {
                                auto res = compile_expr(lines, env, closure, lambda[i], is_root);
                                if (not res) {
                                    return std::unexpected(res.error());
                                }
                                if (i != lambda.size() - 1) {
                                    lines.push_back(Instruction{Mnemonic::pop});
                                }
                            }
                            // NOTE: this relies on the fact that sublambdas are compiled after parent lambdas.
                            env.lambdas_to_skip.emplace(&list[0]);
                            return {};
                        }
                    }

                    auto old_depth = env.stack_depth;
                    for (std::size_t i = 0; i < list.size(); ++i) {
                        auto res = compile_expr(lines, env, closure, list[i], false);
                        if (not res) {
                            return std::unexpected(res.error());
                        }
                        env.stack_depth += 1;
                    }
                    env.stack_depth = old_depth;

                    lines.push_back(Instruction{Mnemonic::indcall, StackOperand{list.size() - 1}});
                    lines.push_back(Instruction{Mnemonic::set, LiteralOperand{1}});
                    return {};
                }
                case ast::ExprType::number:
                case ast::ExprType::string:
                    todo();
            }
            std::unreachable();
        }
        case ast::ExprType::number: {
            auto& number = static_cast<ast::Number&>(expr);
            lines.push_back(Instruction{Mnemonic::push, LiteralOperand{number.value()}});
            return {};
        }
        case ast::ExprType::string:
            todo();
    }
    std::unreachable();
}

std::expected<void, StaticError> compile_closure(std::vector<Line>& lines, Env& env, Closure const& closure) {
    auto& list = closure.source();
    for (std::size_t i = 2; i < list.size(); ++i) {
        auto res = compile_expr(lines, env, closure, list[i], i == list.size() - 1);
        if (not res) {
            return std::unexpected(res.error());
        }
        lines.push_back(Instruction{Mnemonic::pop, std::nullopt});
    }
    lines.pop_back();

    if (closure.header_size() == 0) {
        lines.push_back(Instruction{Mnemonic::ret, LiteralOperand{0}});
    } else {
        auto n_elements = static_cast<std::int64_t>(closure.header_size());
        lines.push_back(Instruction{Mnemonic::setret, LiteralOperand{n_elements}});
    }
    return {};
}

}  // namespace

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
        env.lambdas.insert({&closure->source(), {std::move(name), closure.get()}});
    }

    Code code;
    std::vector<StaticError> errors;
    for (auto& closure : program.closures) {
        if (not closure->parent() or env.lambdas_to_skip.contains(&closure->source())) {
            continue;
        }

        // env.label_cnt = 0;
        env.stack_depth = 0;  // TODO: assert that this is 0 at the end.
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

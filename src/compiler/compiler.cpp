#include "compiler.hpp"

#include <cstddef>
#include <stdexcept>
#include <utility>

#include "ast.hpp"

std::string unsafe_atom_name(ast::Expr const& expr) { return static_cast<ast::Atom const&>(expr).value; }

[[noreturn]] static void todo() { throw std::runtime_error("unimplemented"); }

static CompilationResult compile_quote(std::vector<Line>& code, ast::List& list) {
    if (list.elements[1]->is_atom()) {
        auto& atom = static_cast<ast::Atom&>(*list.elements[1]);
        code.push_back(Instruction{Mnemonic::push, AtomOperand{atom.value}});
        return CompilationResult::ok;
    }

    todo();
}

static CompilationResult compile_expr(std::vector<Line>& code, ast::Expr& expr, int& label_cnt) {
    if (expr.is_list()) {
        auto& list = static_cast<ast::List&>(expr);
        if (list.elements.empty()) {
            code.push_back(Instruction{Mnemonic::push, AtomOperand{"NIL"}});
            return CompilationResult::ok;
        }

        if (not list.elements[0]->is_atom()) {
            return CompilationResult::calling_nonatom;
        }
        auto& callee = static_cast<ast::Atom&>(*list.elements[0]).value;

        auto compile_with_args = [&](Mnemonic m, std::size_t n) {
            // TODO: arity check should be an ICE at this point.
            // This needs to be moved to a static check step at the AST level.
            if (list.elements.size() != n + 1) {
                return CompilationResult::incorrect_arity;
            }
            for (std::size_t i = 0; i < n; i++) {
                auto arg = compile_expr(code, *list.elements[i + 1], label_cnt);
                if (arg != CompilationResult::ok) {
                    return arg;
                }
            }
            code.push_back(Instruction{m});
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
            if (auto cond = compile_expr(code, *list.elements[1], label_cnt); cond != CompilationResult::ok) {
                return cond;
            }
            auto label_false = ".L" + std::to_string(label_cnt++);
            code.push_back(Instruction{Mnemonic::jf, LabelOperand{label_false}});
            if (auto then = compile_expr(code, *list.elements[2], label_cnt); then != CompilationResult::ok) {
                return then;
            }
            auto label_end = ".L" + std::to_string(label_cnt++);
            code.push_back(Instruction{Mnemonic::jmp, LabelOperand{label_end}});
            code.push_back(Label{std::move(label_false)});
            if (auto else_ = compile_expr(code, *list.elements[3], label_cnt); else_ != CompilationResult::ok) {
                return else_;
            }
            code.push_back(Label{label_end});
            return CompilationResult::ok;
        } else if (callee == "print") {
            return compile_with_args(Mnemonic::print, 1);
        } else if (callee == "quote") {
            if (list.elements.size() != 2) {
                return CompilationResult::incorrect_arity;
            }
            return compile_quote(code, list);
        } else {
            bool is_local = false;
            // TODO: replace this with a new parameter for the compiler which holds local names.
            auto it = std::ranges::find_if(code, [](Line const& l) {
                if (auto *label = std::get_if<Label>(&l)) {
                    return label->name.at(0) == '.';
                }
                return false;
            });
            if (it != code.end()) {
                auto& label = *std::get_if<Label>(&*it);
                if (std::ranges::find(label.parameters, callee) != label.parameters.end()) {
                    is_local = true;
                }
            }

            for (std::size_t i = 1; i < list.elements.size(); i++) {
                auto arg = compile_expr(code, *list.elements[i], label_cnt);
                if (arg != CompilationResult::ok) {
                    return arg;
                }
            }
            if (is_local) {
                code.push_back(Instruction{Mnemonic::call, AtomOperand{callee, true}});
            } else {
                code.push_back(Instruction{Mnemonic::call, LabelOperand{callee}});
            }
            return CompilationResult::ok;
        }
    } else if (expr.is_atom()) {
        auto& atom = static_cast<ast::Atom&>(expr);
        code.push_back(Instruction{Mnemonic::push, AtomOperand{atom.value, true}});
        return CompilationResult::ok;
    }

    todo();
}

static CompilationResult compile_define(std::vector<Line>& code, ast::List& list) {
    if (list.elements.size() < 3) {
        return CompilationResult::incorrect_arity;
    }
    auto& params_expr = *list.elements[1];
    if (params_expr.is_list()) {
        auto& params = static_cast<ast::List&>(params_expr);
        if (params.elements.empty()) {
            return CompilationResult::define_missing_name;
        }
        for (auto& param : params.elements) {
            if (not param->is_atom()) {
                return CompilationResult::define_param_is_not_an_atom;
            }
        }

        Label l{.name = unsafe_atom_name(*params.elements[0])};
        for (std::size_t i = 1; i < params.elements.size(); ++i) {
            l.parameters.push_back(unsafe_atom_name(*params.elements[i]));
        }
        code.push_back(l);

        int label_cnt = 0;
        for (std::size_t i = 2; i < list.elements.size(); ++i) {
            auto res = compile_expr(code, *list.elements[i], label_cnt);
            if (res != CompilationResult::ok) {
                return res;
            }
            if (i != list.elements.size() - 1) {
                code.push_back(Instruction{Mnemonic::pop});
            }
        }
        if (params.elements.size() == 1) {
            code.push_back(Instruction{Mnemonic::ret, LiteralOperand{0}});
        } else {
            auto n_params = static_cast<std::int64_t>(params.elements.size() - 1);
            code.push_back(Instruction{Mnemonic::set, LiteralOperand{n_params}, LiteralOperand{0, true}});
            code.push_back(Instruction{Mnemonic::ret, LiteralOperand{n_params}});
        }
        return CompilationResult::ok;
    }
    todo();
}

CompilationResult compile(std::vector<Line>& code, ast::Expr& expr) {
    if (not expr.is_list()) {
        todo();
    }
    auto& list = static_cast<ast::List&>(expr);

    if (list.elements.empty()) {
        return CompilationResult::ok;
    }

    auto& function_expr = *list.elements.front();
    if (not function_expr.is_atom()) {
        return CompilationResult::calling_nonatom;
    }
    auto& function = static_cast<ast::Atom&>(function_expr);

    if (function.value == "define") {
        return compile_define(code, list);
    }

    todo();
}

static std::string to_string(AtomOperand const& o) {
    if (o.is_stack) {
        return '[' + o.name + ']';
    }
    return '\'' + o.name;
}

static std::string to_string(LiteralOperand const& o) {
    if (o.is_stack) {
        return '[' + std::to_string(o.value) + ']';
    }
    return std::to_string(o.value);
}

static std::string to_string(LabelOperand const& o) { return o.name; }

static std::string to_string(Operand const& o) {
    return std::visit([](auto&& x) { return to_string(x); }, o);
}

std::string format_line(Line const& line) {
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
        auto operator()(Label label) {
            auto r = label.name;
            for (auto& p : label.parameters) {
                r += ' ' + p;
            }
            return r + ':';
        }
    };
    return std::visit(Visitor{}, line);
}

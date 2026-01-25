#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

#define FWD(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

template <typename... Ts>
struct overload : Ts... {
    using Ts::operator()...;
};

#include "alloc.hpp"
#include "eval.hpp"
#include "lisp_parser.hpp"

extern "C" {
#include <readline/history.h>
#include <readline/readline.h>
}

struct FreeDeleter {
    static void operator()(void *ptr) { std::free(ptr); }
};

int main() {
    GC gc;
    Alloc alloc(gc);
    auto p = parse::s_expr(alloc);
    Env env;

    using_history();

    while (auto input = std::unique_ptr<char, FreeDeleter>(readline("> "))) {
        add_history(input.get());

        std::visit(overload{
                       [&](parse::Parser<ast::Expr *>::Ok r) {
                           if (not r.rest.empty()) {
                               std::cout << "[Warning] trailing_characters: \"" << r.rest << "\"\n";
                           }
                           std::cout << "[Debug] prog: " << r.value->format() << '\n';
                           try {
                               std::cout << eval(r.value, env, alloc)->format() << '\n';
                           } catch (std::exception const& err) {
                               std::cout << "Runtime error: " << err.what() << '\n';
                           }
                           gc.collect();

                           std::cout << "GC nodes: " << gc.debug() << '\n';
                       },
                       [](parse::Err e) {
                           std::cout << "Error at " << e.where.line << ", " << e.where.col << ": " << e.what << '\n';
                       },
                   },
                   p.run(input.get(), parse::Pos{1, 1}));
    }
}

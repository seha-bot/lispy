#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <utility>

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

        if (auto r = p.run(input.get(), parse::Pos{1, 1})) {
            if (not r->rest.empty()) {
                std::cout << "[Warning] trailing_characters: \"" << r->rest << "\"\n";
            }
            std::cout << "[Debug] prog: " << r->value->format() << '\n';
            try {
                std::cout << eval(r->value, env, alloc)->format() << '\n';
            } catch (std::exception const& err) {
                std::cout << "Runtime error: " << err.what() << '\n';
            }
            gc.collect();

            std::cout << "GC nodes: " << gc.debug() << '\n';
        } else {
            auto [what, where] = r.error();
            std::cout << "Error at " << where.line << ", " << where.col << ": " << what << '\n';
        }
    }
}

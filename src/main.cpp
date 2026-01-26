#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
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

GC gc;
Alloc alloc(gc);
auto p = parse::s_expr(alloc);
Env env;

parse::Parser<ast::Expr *>::Ok run(std::string_view input, parse::Pos pos) {
    if (auto r = p.run(input, pos)) {
        try {
            std::cout << "[Debug] prog: " << r->value->format() << '\n';
            eval(r->value, env, alloc);
        } catch (std::exception const& err) {
            std::cout << "Runtime error: " << err.what() << '\n';
        }
        gc.collect();
        return *r;
    } else {
        auto [what, where] = r.error();
        std::cout << "[Error] at " << where.line << ':' << where.col << ": " << what << '\n';
        throw std::runtime_error("yeah");
    }
}

int main(int argc, char *argv[]) {
    // {auto p = parse::ignorable();
    //     std::cout << p.run(";; hello\n;; wazzup\n", parse::Pos{1, 1}).value().rest;
    //     return 0;
    // }

    // argc = 2;
    // argv[1] = "/home/seha/repos/lispy/prelude.lsp";
    if (argc == 2) {
        std::ifstream f(argv[1]);
        if (not f) {
            throw std::runtime_error("file something");
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        auto s = buffer.str();
        // std::cout << s << '\n';
        std::string_view sv = s;
        parse::Pos pos{1, 1};
        while (not sv.empty()) {
            auto r = run(sv, pos);
            sv = r.rest;
            pos = r.where;
        }
    }

    using_history();

    while (auto input = std::unique_ptr<char, FreeDeleter>(readline("> "))) {
        add_history(input.get());

        if (auto r = p.run(input.get(), parse::Pos{1, 1})) {
            if (not r->rest.empty()) {
                std::cout << "[Warning] trailing_characters: \"" << r->rest << "\"\n";
            }
            // std::cout << "[Debug] prog: " << r->value->format() << '\n';
            try {
                eval(r->value, env, alloc);
                std::cout << '\n';
                // std::cout << eval(r->value, env, alloc)->format() << '\n';
            } catch (std::exception const& err) {
                std::cout << "Runtime error: " << err.what() << '\n';
            }
            gc.collect();

            // std::cout << "[Debug] GC nodes: " << gc.debug() << '\n';
        } else {
            auto [what, where] = r.error();
            std::cout << "[Error] at " << where.line << ':' << where.col << ": " << what << '\n';
        }
    }
}

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
auto p = parse::ws() > parse::s_expr(alloc) < parse::ws();
Env env;

std::string_view run(std::string_view input) {
    if (auto r = p.run(input, parse::Pos{1, 1})) {
        try {
            eval(r->value, env, alloc);
        } catch (std::exception const& err) {
            std::cout << "Runtime error: " << err.what() << '\n';
        }
        gc.collect();
        return r->rest;
    } else {
        auto [what, where] = r.error();
        std::cout << "[Error] at " << where.line << ", " << where.col << ": " << what << '\n';
        throw std::runtime_error("yeah");
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        std::ifstream f(argv[1]);
        if (not f) {
            throw std::runtime_error("file something");
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        auto s = buffer.str();
        std::string_view sv = s;
        while (not sv.empty()) {
            sv = run(sv);
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
            std::cout << "[Error] at " << where.line << ", " << where.col << ": " << what << '\n';
        }
    }
}

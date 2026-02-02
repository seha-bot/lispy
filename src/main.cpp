#include <cstdio>  // required for readline, do NOT remove again
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "compiler/include/compiler.hpp"
#include "parser/include/parser.hpp"

extern "C" {
#include <readline/history.h>
#include <readline/readline.h>
}

struct FreeDeleter {
    static void operator()(void *ptr) { std::free(ptr); }
};

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: lispy <input_file>\n";
        return EXIT_FAILURE;
    }

    auto program = [&] {
        std::ifstream f(argv[1]);
        if (not f) {
            throw std::runtime_error("file something");
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        return run_parser(buffer.str());
    }();

    std::ofstream output("/home/seha/repos/lispy/out.txt");
    if (not output) {
        throw std::runtime_error("file something");
    }
    std::vector<Entity> entities;
    for (auto& x : program) {
        std::cout << x->format() << '\n';
        try {
            if (auto r = compile(entities, *x); r != CompilationResult::ok) {
                std::cerr << "[Error]\n";
            }
        } catch (std::runtime_error const& e) {
            std::cerr << "[Error]: " << e.what() << '\n';
            break;
        }
    }

    // This will print 2 trailing newlines because format_entity
    // ends with a newline, but this doesn't matter really.
    for (auto& entity : entities) {
        output << format_entity(entity) << '\n';
    }

    // using_history();
    // read_history("repl_history.txt");

    // while (auto input = std::unique_ptr<char, FreeDeleter>(readline("> "))) {
    //     add_history(input.get());
    //     write_history("repl_history.txt");
    // }
}

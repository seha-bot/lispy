#include <cstdio>  // required for readline, do NOT remove again
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include "compiler/include/compiler.hpp"
#include "compiler/include/explorer.hpp"
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

    auto source = [&] {
        std::ifstream f(argv[1]);
        if (not f) {
            throw std::runtime_error("file something");
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        return buffer.str();
    }();

    std::ofstream output("/home/seha/repos/lispy/out.txt");
    if (not output) {
        throw std::runtime_error("file something");
    }

    auto program = run_parser(source);
    if (not program) {
        std::cerr << program.error() << '\n';
        return EXIT_FAILURE;
    }

    auto program_res = do_program(*program);
    if (not program_res) {
        auto& errs = program_res.error();
        for (auto& err : errs) {
            std::cerr << err << '\n';
        }
        return EXIT_FAILURE;
    }

    auto code_res = compiler::compile_program(*program_res);
    if (not code_res) {
        auto& errs = code_res.error();
        for (auto& err : errs) {
            std::cerr << err << '\n';
        }
        return EXIT_FAILURE;
    }

    output << *code_res;

    // using_history();
    // read_history("repl_history.txt");

    // while (auto input = std::unique_ptr<char, FreeDeleter>(readline("> "))) {
    //     add_history(input.get());
    //     write_history("repl_history.txt");
    // }
}

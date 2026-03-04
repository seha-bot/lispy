#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "compiler/include/lowerer.hpp"
#include "parser/include/parser.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: lispy <source>\n";
        return EXIT_FAILURE;
    }
    // can std::filesystem be used here maybe?
    std::string const filename = argv[1];

    std::string source;
    {
        std::ifstream const f(filename);
        if (not f) {
            std::cerr << "Can't open file \"" << filename << "\" for reading.";
            return EXIT_FAILURE;
        }
        std::stringstream buffer;
        buffer << f.rdbuf();
        source = std::move(buffer).str();
    }

    // std::ofstream output(filename + ".out");
    // if (not output) {
    //     std::cerr << "Can't open file \"" << filename << ".out" << "\" for writing.";
    //     return EXIT_FAILURE;
    // }

    auto raw_ast = parse_source(source);
    if (not raw_ast) {
        std::cerr << raw_ast.error() << '\n';
        return EXIT_FAILURE;
    }

    auto result = compiler::lower_ast(filename, *std::move(raw_ast));
    if (not result) {
        std::cerr << result.error() << '\n';
        return EXIT_FAILURE;
    }
}

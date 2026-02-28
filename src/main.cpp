#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

#include "compiler/include/explorerv2.hpp"
#include "compiler/include/name_resolver.hpp"
#include "parser/include/parser.hpp"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: lispy <source>\n";
        return EXIT_FAILURE;
    }
    std::string filename = argv[1];

    std::string source;
    {
        std::ifstream f(filename);
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

    auto ast = run_parser(source);
    if (not ast) {
        std::cerr << ast.error() << '\n';
        return EXIT_FAILURE;
    }

    auto block = definitions::discover(*std::move(ast));
    if (not block) {
        std::cerr << block.error() << '\n';
        return EXIT_FAILURE;
    }

    auto idk = names::resolve(*std::move(block));
}

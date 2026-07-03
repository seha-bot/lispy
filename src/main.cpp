#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

#include "analyser/include/typechecker.hpp"
#include "emitter/include/emitter.hpp"
#include "parser/include/lowerer.hpp"
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

  auto raw_ast = parser::parse_source(source);
  if (not raw_ast) {
    std::cerr << raw_ast.error() << '\n';
    return EXIT_FAILURE;
  }

  auto ast = parser::lower_ast(filename, *std::move(raw_ast));
  if (not ast) {
    std::cerr << ast.error() << '\n';
    return EXIT_FAILURE;
  }

  auto type_env = analyser::typecheck(*ast);
  if (not type_env) {
    std::cerr << type_env.error() << '\n';
    return EXIT_FAILURE;
  }

  emitter::emit(*ast, *type_env, std::cout);
}

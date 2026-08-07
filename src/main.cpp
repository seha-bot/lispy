#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>

import formatter;
import parser;
import typechecker;

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

  auto ast = parser::parse(source);
  if (not ast) {
    std::cerr << ast.error() << '\n';
    return EXIT_FAILURE;
  }

  std::cout << "STRUCTURED AST START.\n";
  for (auto &entity : ast->entities) {
    formatter::format_entity(std::cout, {*ast->ts, ast->entities, ast->forms, ast->tags}, 0,
                             entity);
    std::cout << '\n';
  }
  std::cout << "STRUCTURED AST END.\n";

  auto type_env = analyser::typecheck(*ast->ts, ast->tags, ast->forms, std::move(ast->entities));
  if (not type_env) {
    std::cerr << type_env.error() << '\n';
    return EXIT_FAILURE;
  }

  // emitter::emit(*ast, *type_env, std::cout);
}

#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <utility>

import parser;
import typechecker;

auto parse(std::string_view expr) {
  auto ast = parser::parse(expr);
  REQUIRE(ast.has_value());
  return *std::move(ast);
}

TEST_CASE("Error on unconstrained types.", "[typechecker]") {
  auto ast = parse("(def id (lambda x x))");
  auto result = analyser::typecheck(*ast.ts, ast.tags, ast.forms, std::move(ast.entities));
  REQUIRE_FALSE(result.has_value());
}

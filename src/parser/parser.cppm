module;

#include <expected>
#include <sstream>
#include <string>
#include <string_view>

export module parser;

import parsy;
import raw_ast_parser;
import source_parser;

namespace parser {

export std::expected<raw_ast_parser::ResolvedAST, std::string> parse(std::string_view source) {
  auto raw_ast = source_parser::parse(source);
  if (not raw_ast) {
    std::ostringstream os;
    os << raw_ast.error();
    return std::unexpected(std::move(os).str());
  }

  auto ast = raw_ast_parser::parse(*std::move(raw_ast));
  if (not ast) {
    std::ostringstream os;
    os << ast.error();
    return std::unexpected(std::move(os).str());
  }

  return *std::move(ast);
}

} // namespace parser

#ifndef PARSER_HPP
#define PARSER_HPP

#include <cstddef>
#include <expected>
#include <vector>

#include "parsy.hpp"
#include "raw_ast.hpp"

namespace parser {

struct StringView {
  StringView(std::string_view view, raw_ast::SourceLocation pos) : m_view(view), m_pos(pos) {}

  raw_ast::SourceLocation pos() const { return m_pos; }
  bool empty() const { return m_view.empty(); }
  StringView take(std::size_t n) const { return {m_view.substr(0, n), m_pos}; }
  char head() const { return m_view.at(0); }

  StringView next() const {
    auto new_pos = m_pos;
    if (m_view.at(0) == '\n') {
      ++new_pos.line;
      new_pos.col = 0;
    }
    ++new_pos.col;
    return {m_view.substr(1), new_pos};
  }

  std::string to_string() const { return std::string(m_view); }

private:
  std::string_view m_view;
  raw_ast::SourceLocation m_pos;
};

// TODO: Hide the parsy dependency from this header and move StringView into parser.cpp.
std::expected<std::vector<raw_ast::Expr>, parsy::ParseError<StringView>>
parse_source(std::string_view input) noexcept;

} // namespace parser

#endif

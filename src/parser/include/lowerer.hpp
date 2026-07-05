#ifndef LOWERER_HPP
#define LOWERER_HPP

#include "parsy.hpp"
#include "raw_ast.hpp"
#include "storage/resolved.hpp"
#include <expected>
#include <string>
#include <vector>

namespace parser {

struct ExprView {
  ExprView(std::span<raw_ast::Expr> view, raw_ast::SourceLocation last_seen)
      : m_view(view), m_range(last_seen, last_seen) {
    if (not view.empty()) {
      m_range = view.front().source_range();
    }
  }

  raw_ast::SourceRange pos() const { return m_range; }
  bool empty() const { return m_view.empty(); }
  ExprView take(std::size_t n) const { return {m_view.subspan(0, n), m_range.first}; }
  raw_ast::Expr &head() { return m_view[0]; }
  raw_ast::Expr const &head() const { return m_view[0]; }

  // TODO: This works, but the m_range manipulation could end up somewhere outside of your code.
  // Example:
  // (def id
  // (lambda (x
  // )x))
  // The error will complain about line 2 right after x (missing type) and while that is correct,
  // it is not inside your code. You could make it point at the closing parenthesis if you want
  // or do something else.
  ExprView next() const { return {m_view.subspan(1), {m_range.last.line, m_range.last.col + 1}}; }

  raw_ast::Atom &head_as_atom() { return *std::get_if<raw_ast::Atom>(&head()); }

private:
  std::span<raw_ast::Expr> m_view;
  raw_ast::SourceRange m_range;
};

std::expected<storage::ResolvedAST, parsy::ParseError<ExprView>>
lower_ast(std::string filename, std::vector<raw_ast::Expr> ast) noexcept;

} // namespace parser

#endif

module;

#include <array>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module parser;

import parsy;
import raw_ast;

namespace {

struct StringView {
  StringView(std::string_view view, raw_ast::SourceLocation pos) : m_view(view), m_pos(pos) {}

  // TODO: This should probably also be a SourceRange.
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

bool is_space(char c) { return c == ' ' or c == '\n' or c == '\t' or c == '\r'; }

template <typename A> using Parser = parsy::Parsy<StringView, A>;

Parser<StringView> char_(char c) {
  return satisfies<StringView>(parsy::MeaningfulPredicate{
      .meaning = '\'' + std::string(1, c) + '\'',
      .fn = [c](char matched_c) -> bool { return matched_c == c; },
  });
};

Parser<StringView> ws() {
  return parsy::many_where<StringView>([](char c) -> bool { return is_space(c); });
}

Parser<StringView> wsc() {
  auto comment_start = [] {
    return satisfies<StringView>(parsy::MeaningfulPredicate{
        .meaning = "a comment",
        .fn = [](char c) -> bool { return c == ';'; },
    });
  };
  auto comment_content = [] {
    return parsy::many_where<StringView>([](char c) -> bool { return c != '\n'; });
  };

  // TODO: No need for this to be allocating.
  return ws() < many(comment_start() > comment_content() > ws());
}

Parser<StringView> atom_content() {
  return some_where<StringView>(parsy::MeaningfulPredicate{
      .meaning = "an atom",
      .fn = [](char c) -> bool { return not is_space(c) and c != '(' and c != ')' and c != ';'; },
  });
};

Parser<StringView> lparen() { return char_('('); };
Parser<StringView> rparen() { return char_(')'); };

// TODO: this is duplicated.
template <typename T>
auto const to = []<typename... Ts>(Ts &&...args) { return T(std::forward<Ts>(args)...); };

Parser<raw_ast::Expr> expr() {
  auto atom = atom_content() | [](StringView text) -> raw_ast::Expr {
    auto name = text.to_string();
    auto size = static_cast<int>(name.size());
    return {
        raw_ast::Atom{.name = std::move(name)},
        {
            .first = text.pos(),
            .last = {text.pos().line, text.pos().col + size - 1},
        },
    };
  };

  auto to_list = [](StringView lparen, std::vector<raw_ast::Expr> exprs,
                    StringView rparen) -> raw_ast::Expr {
    return {
        raw_ast::List(std::move(exprs)),
        {lparen.pos(), rparen.pos()},
    };
  };
  auto rec_expr = [] { return rec<StringView>(expr); };
  auto list = seq(to_list, lparen(), cut(wsc() > many(rec_expr() < wsc())), cut(rparen()));

  using parsy::errors::Error, parsy::errors::Custom;
  auto error = [](StringView tokens, Error<StringView> const &) {
    auto [line, col] = tokens.pos();
    return Custom{
        .msg = "Unclosed '(' at " + std::to_string(line) + ':' + std::to_string(col) + '.',
    };
  };

  return any<StringView>(std::array{
      std::move(atom),
      map_error(error, std::move(list)),
  });
};

} // namespace

export namespace parser {

std::expected<std::vector<raw_ast::Expr>, parsy::ParseError<StringView>>
parse_source(std::string_view input) noexcept {
  return parse(wsc() > many(expr() < wsc()), StringView(input, raw_ast::SourceLocation{1, 1}));
}

} // namespace parser

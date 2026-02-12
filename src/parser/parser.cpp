#include "parser.hpp"

#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ast.hpp"

namespace {

enum class TokenType {
    unknown,
    open_parenthesis,
    close_parenthesis,
    quote,
    identifier,
    number,
    string,
};

struct Token {
    Token(std::string_view::iterator begin, std::string_view::iterator end, TokenType type, int line, int col)
        : m_begin(begin), m_end(end), m_type(type), m_line(line), m_col(col) {}

    std::string_view text() const { return std::string_view(m_begin, m_end); }
    TokenType type() const { return m_type; }

    Token with_extent(std::optional<Token> that) const {
        if (not that) {
            return *this;
        }

        if (m_end != that->m_begin) {
            throw std::logic_error("Internal parser error.");
        }
        return Token{m_begin, that->m_end, m_type, m_line, m_col};
    }
    Token with_type(TokenType type) const { return Token(m_begin, m_end, type, m_line, m_col); }

private:
    // TODO: remove this and make a proper API.
    friend struct Lexer;
    std::string_view::iterator m_begin, m_end;
    TokenType m_type;
    int m_line, m_col;
};

struct Lexer {
    Lexer(std::string_view input) : m_input(input), m_state{0, 1, 1} {}

    std::expected<Token, ParseError> next_token() {
        while (true) {
            {
                auto whitespace = while_([](char c) -> bool { return std::isspace(c); });
                if (not whitespace) {
                    return std::unexpected(whitespace.error());
                }
            }

            char first = 0;
            {
                auto r = peek();
                if (not r) {
                    return r;
                }
                first = r->text()[0];
            }

            switch (first) {
                case '(':
                    return char_().transform([](Token tok) { return tok.with_type(TokenType::open_parenthesis); });
                case ')':
                    return char_().transform([](Token tok) { return tok.with_type(TokenType::close_parenthesis); });
                case '\'':
                    return char_().transform([](Token tok) { return tok.with_type(TokenType::quote); });
                case ';': {
                    auto comment = while_([](char c) { return c != '\n'; });
                    if (not comment) {
                        return std::unexpected(comment.error());
                    }
                    break;
                }
                case '"':
                    // TODO: add escaping and error reporting
                    return char_().and_then([&](Token open) {
                        return while_([](char c) { return c != '"'; }).and_then([&](std::optional<Token> str) {
                            return char_().transform([&](Token close) {
                                return open.with_extent(str).with_extent(close).with_type(TokenType::string);
                            });
                        });
                    });
                default:
                    if (is_digit(first)) {
                        return while_(is_digit).transform([](std::optional<Token> tok) {  //
                            return tok->with_type(TokenType::number);
                        });
                    } else {
                        return while_(is_identifier).transform([](std::optional<Token> tok) {
                            return tok->with_type(TokenType::identifier);
                        });
                    }
            }
        }
    }

    ast::Source checkpoint() const { return m_state; }
    void rewind(ast::Source checkpoint) { m_state = checkpoint; }

    ast::Source source_location(Token tok) const {
        return ast::Source{static_cast<std::size_t>(tok.m_begin - m_input.begin()), tok.m_line, tok.m_col};
    }

private:
    static bool is_digit(char c) { return std::isdigit(c); }
    static bool is_identifier(char c) {
        return not std::isspace(c) and c != '(' and c != ')' and c != '\'' and c != ';' and c != '"';
    }

    std::expected<Token, ParseError> char_() {
        if (m_state.position == m_input.size()) {
            return std::unexpected(ParseError{ParseError::end_of_input, m_state});
        }
        Token tok(m_input.begin() + m_state.position, m_input.begin() + m_state.position + 1, TokenType::unknown,
                  m_state.line, m_state.col);
        char const c = m_input[m_state.position++];
        if (c == '\n') {
            m_state.line += 1;
            m_state.col = 0;
        }
        m_state.col += 1;
        return tok;
    }

    std::expected<Token, ParseError> peek() const {
        if (m_state.position == m_input.size()) {
            return std::unexpected(ParseError{ParseError::end_of_input, m_state});
        }
        return Token(m_input.begin() + m_state.position, m_input.begin() + m_state.position + 1, TokenType::unknown,
                     m_state.line, m_state.col);
    }

    std::expected<std::optional<Token>, ParseError> while_(bool (*pred)(char)) {
        std::optional<Token> res;
        while (true) {
            auto const c = peek();
            if (not c) {
                return c;
            }
            if (not pred(c->text()[0])) {
                return res;
            }
            if (not res) {
                res = char_().value();
            } else {
                res = res->with_extent(char_().value());
            }
        }
    }

    std::string_view m_input;
    ast::Source m_state;
};

std::expected<ast::ExprPtr, ParseError> parse_expr(Lexer& lex) {
    auto const token = lex.next_token();
    if (not token) {
        return std::unexpected(token.error());
    }

    auto source_location = lex.source_location(*token);
    switch (token->type()) {
        case TokenType::unknown:
            throw std::logic_error("Internal parser error.");
        case TokenType::open_parenthesis: {
            std::vector<ast::ExprPtr> list;
            while (true) {
                auto const checkpoint = lex.checkpoint();
                auto const peek = lex.next_token();
                if (not peek) {
                    return std::unexpected(peek.error());
                }
                if (peek->type() == TokenType::close_parenthesis) {
                    break;
                }
                lex.rewind(checkpoint);

                auto subexpr = parse_expr(lex);
                if (not subexpr) {
                    return subexpr;
                }
                list.push_back(*std::move(subexpr));
            }
            return std::make_unique<ast::List>(std::move(list), source_location);
        }
        case TokenType::close_parenthesis:
            return std::unexpected(ParseError{ParseError::mismatched_parentheses, source_location});
        case TokenType::quote: {
            auto subexpr = parse_expr(lex);
            if (not subexpr) {
                return subexpr;
            }
            std::vector<ast::ExprPtr> list;
            list.push_back(std::make_unique<ast::Atom>("quote", source_location));
            list.push_back(*std::move(subexpr));
            return std::make_unique<ast::List>(std::move(list), source_location);
        }
        case TokenType::identifier:
            return std::make_unique<ast::Atom>(std::string(token->text()), source_location);
        case TokenType::number: {
            // TODO: what if the number doesn't fit
            std::int64_t v = 0;
            auto const text = token->text();
            std::from_chars(text.data(), text.data() + text.size(), v);
            return std::make_unique<ast::Number>(v, source_location);
        }
        case TokenType::string:
            return std::make_unique<ast::String>(std::string(token->text()), source_location);
    }
    std::unreachable();
}

}  // namespace

std::expected<std::vector<ast::ExprPtr>, ParseError> run_parser(std::string_view input) {
    Lexer lex(input);
    std::vector<ast::ExprPtr> exprs;
    while (true) {
        auto expr = parse_expr(lex);
        if (not expr) {
            if (expr.error().what == ParseError::end_of_input) {
                return exprs;
            }
            return std::unexpected(expr.error());
        }
        exprs.push_back(*std::move(expr));
    }
}

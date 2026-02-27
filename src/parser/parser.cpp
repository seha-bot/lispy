#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <expected>
#include <iostream>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "ast.hpp"
#include "todo.hpp"

namespace {

struct UntypedToken {
    UntypedToken(std::string_view::iterator begin, std::string_view::iterator end, rast::Source source)
        : m_begin(begin), m_end(end), m_source(source) {}

    std::string_view text() const { return std::string_view(m_begin, m_end); }

    UntypedToken concat(UntypedToken that) const {
        if (m_end != that.m_begin) {
            std::cerr << "Internal error. Report immediately.\n";
            std::terminate();
        }
        return UntypedToken(m_begin, that.m_end, m_source);
    }

    rast::Source source_location() const noexcept { return m_source; }

private:
    std::string_view::iterator m_begin, m_end;
    rast::Source m_source;
};

enum class TokenType {
    eof,
    left_parenthesis,
    right_parenthesis,
    quote,
    text,
    string,
};

struct Token : UntypedToken {
    Token(UntypedToken token, TokenType type) : UntypedToken(token), m_type(type) {}

    TokenType type() const { return m_type; }

private:
    TokenType m_type;
};

struct Lexer {
    Lexer(std::string_view input) : m_input(input) {}

    std::expected<Token, ParseError> next_token() noexcept {
        auto const cp = checkpoint();
        auto const tok = char_();
        if (not tok or tok->text()[0] == '\n') {
            if (has_dollar()) {
                rewind(cp);
                pop_dollar();
                return Token(empty(), TokenType::right_parenthesis);
            }
            if (has_colon()) {
                while_(is_space);

                bool is_empty_line = false;
                {
                    auto const next_cp = checkpoint();
                    auto next_tok = char_();
                    if (next_tok and next_tok->text()[0] == '\n') {
                        is_empty_line = true;
                    }
                    rewind(next_cp);
                }

                if (not tok or (m_source.col <= top_colon() and not is_empty_line)) {
                    rewind(cp);
                    pop_colon();
                    return Token(empty(), TokenType::right_parenthesis);
                }
            }
            return tok ? next_token() : Token(empty(), TokenType::eof);
        }
        char const first = tok->text()[0];

        switch (first) {
            case '(':
                return Token(*tok, TokenType::left_parenthesis);
            case ')':
                return Token(*tok, TokenType::right_parenthesis);
            case '\'':
                return Token(*tok, TokenType::quote);
            case ':':
                push_colon(*tok);
                return Token(*tok, TokenType::left_parenthesis);
            case '$':
                push_dollar();
                return Token(*tok, TokenType::left_parenthesis);
            case ';':
                while_([](char c) { return c != '\n'; });
                return next_token();
            case '"':
                // TODO: add escaping and error reporting
                // return while_([](char c) { return c != '"'; }).and_then([&](Token str) {
                //     return char_().transform([&](Token close) {
                //         return tok->with_extent(str).with_extent(close).with_type(TokenType::string);
                //     });
                // });
                todo();
            default:
                if (is_text(first)) {
                    rewind(cp);
                    return Token(*while_(is_text), TokenType::text);
                } else if (is_space(first)) {
                    while_(is_space);
                    return next_token();
                } else {
                    return std::unexpected(ParseError{ParseError::unexpected_token, tok->source_location()});
                }
        }
    }

    std::pair<std::size_t, rast::Source> checkpoint() const noexcept { return {m_position, m_source}; }
    void rewind(std::pair<std::size_t, rast::Source> checkpoint) noexcept {
        m_position = checkpoint.first;
        m_source = checkpoint.second;
    }

private:
    static bool is_space(char c) { return c == ' '; }
    static bool is_text(char c) { return std::isalnum(c) or std::string_view{".!%&*/<=>?~_^|+-,\\@#"}.contains(c); }

    void push_dollar() { ++m_dollar_stack; }
    bool has_dollar() const { return m_dollar_stack != 0; }
    void pop_dollar() { --m_dollar_stack; }

    void push_colon(UntypedToken tok) { m_colon_stack.push_back(tok.source_location().col); }
    bool has_colon() const { return not m_colon_stack.empty(); }
    void pop_colon() { m_colon_stack.pop_back(); }
    int top_colon() const { return m_colon_stack.back(); }

    UntypedToken empty() const noexcept { return UntypedToken(m_input.end(), m_input.end(), m_source); }

    std::optional<UntypedToken> char_() noexcept {
        if (m_position == m_input.size()) {
            return std::nullopt;
        }
        auto const it = m_input.begin() + m_position;
        UntypedToken const tok(it, it + 1, m_source);
        char const c = m_input[m_position++];
        if (c == '\n') {
            m_source.line += 1;
            m_source.col = 0;
        }
        m_source.col += 1;
        return tok;
    }

    std::optional<UntypedToken> while_(bool (*pred)(char)) {
        std::optional<UntypedToken> res;
        while (true) {
            auto const cp = checkpoint();
            auto const tok = char_();
            if (not tok or not pred(tok->text()[0])) {
                rewind(cp);
                return res;
            }
            if (not res) {
                res = tok;
            } else {
                res = res->concat(*tok);
            }
        }
    }

    std::vector<int> m_colon_stack;
    std::size_t m_dollar_stack = 0;
    std::string_view m_input;
    std::size_t m_position = 0;
    rast::Source m_source{1, 1};
};

static bool is_digit(char c) { return std::isdigit(c); };

std::expected<rast::ExprPtr, ParseError> parse_expr(Lexer& lex) noexcept {
    auto const token = lex.next_token();
    if (not token) {
        return std::unexpected(token.error());
    }

    auto const source_location = token->source_location();
    switch (token->type()) {
        case TokenType::eof:
            return std::unexpected(ParseError{ParseError::end_of_input, source_location});
        case TokenType::left_parenthesis: {
            std::vector<rast::ExprPtr> list;
            while (true) {
                auto subexpr = parse_expr(lex);
                if (not subexpr) {
                    if (subexpr.error().what == ParseError::mismatched_parentheses) {
                        break;
                    }
                    return subexpr;
                }
                list.push_back(*std::move(subexpr));
            }
            return std::make_unique<rast::List>(std::move(list), source_location);
        }
        case TokenType::right_parenthesis:
            return std::unexpected(ParseError{ParseError::mismatched_parentheses, source_location});
        case TokenType::quote: {
            auto subexpr = parse_expr(lex);
            if (not subexpr) {
                return subexpr;
            }
            std::vector<rast::ExprPtr> list;
            list.push_back(std::make_unique<rast::Atom>("quote", source_location));
            list.push_back(*std::move(subexpr));
            return std::make_unique<rast::List>(std::move(list), source_location);
        }
        case TokenType::text:
            if (is_digit(token->text()[0])) {
                if (not std::ranges::all_of(token->text(), is_digit)) {
                    return std::unexpected(ParseError{ParseError::invalid_digit, source_location});
                }
                std::int64_t v = 0;
                auto const text = token->text();
                // TODO: check for errors
                std::from_chars(text.data(), text.data() + text.size(), v);
                return std::make_unique<rast::Number>(v, source_location);
            } else {
                return std::make_unique<rast::Atom>(std::string(token->text()), source_location);
            }
        case TokenType::string:
            return std::make_unique<rast::String>(std::string(token->text()), source_location);
    }
    std::unreachable();
}

}  // namespace

std::expected<std::vector<rast::ExprPtr>, ParseError> run_parser(std::string_view input) noexcept {
    Lexer lex(input);
    std::vector<rast::ExprPtr> exprs;
    while (true) {
        {
            Lexer peeker(input);
            peeker.rewind(lex.checkpoint());
            auto const tok = peeker.next_token();
            if (not tok) {
                return std::unexpected(tok.error());
            }
            if (tok->type() == TokenType::eof) {
                break;
            }
        }

        auto expr = parse_expr(lex);
        if (not expr) {
            return std::unexpected(expr.error());
        }
        exprs.push_back(*std::move(expr));
    }
    return exprs;
}

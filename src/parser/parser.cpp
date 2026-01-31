#include "parser.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

enum class TokenType { parenthesis, quote, identifier, number, string };

struct Token {
    std::string_view text;
    TokenType type;
};

template <typename T>
using Result = std::optional<std::pair<T, std::string_view>>;

static Result<Token> next_token(std::string_view input) {
    while (true) {
        auto it = std::ranges::find_if(input, [](char c) { return not std::isspace(c); });
        if (it == input.end()) {
            return std::nullopt;
        }
        input = std::string_view(it, input.end());

        switch (input[0]) {
            case '(':
            case ')':
                return std::make_pair(Token{input.substr(0, 1), TokenType::parenthesis}, input.substr(1));
            case '\'':
                return std::make_pair(Token{input.substr(0, 1), TokenType::quote}, input.substr(1));
            case ';':
                input = std::string_view(std::ranges::find(input, '\n'), input.end());
                break;
            case '"': {
                input = input.substr(1);
                auto it = std::ranges::find(input, '"');
                auto length = static_cast<std::size_t>(it - input.begin());
                return std::make_pair(Token{input.substr(0, length), TokenType::string}, input.substr(length));
            }
            default:
                if (std::isdigit(input[0])) {
                    auto it = std::ranges::find_if(input, [](char c) { return not std::isdigit(c); });
                    auto length = static_cast<std::size_t>(it - input.begin());
                    return std::make_pair(Token{input.substr(0, length), TokenType::number}, input.substr(length));
                } else {
                    auto it = std::ranges::find_if(
                        input, [](char c) { return std::isspace(c) or c == '(' or c == ')' or c == '\''; });
                    auto length = static_cast<std::size_t>(it - input.begin());
                    auto text = input.substr(0, length);
                    return std::make_pair(Token{text, TokenType::identifier}, input.substr(length));
                }
        }
    }
}

static Result<std::unique_ptr<ast::Expr>> parse_expr(std::string_view input) {
    auto token_res = next_token(input);
    if (not token_res) {
        return std::nullopt;
    }
    auto t = token_res->first;
    input = token_res->second;

    switch (t.type) {
        case TokenType::parenthesis: {
            if (t.text == ")") {
                return std::nullopt;
            }
            std::vector<std::unique_ptr<ast::Expr>> list;
            while (true) {
                auto peek_token_res = next_token(input);
                if (not peek_token_res) {
                    return std::nullopt;
                }
                auto peek_token = peek_token_res->first;
                if (peek_token.type == TokenType::parenthesis and peek_token.text == ")") {
                    input = peek_token_res->second;
                    break;
                }

                auto subexpr_res = parse_expr(input);
                if (not subexpr_res) {
                    return std::nullopt;
                }
                auto [subexpr, rem] = *std::move(subexpr_res);
                list.push_back(std::move(subexpr));
                input = rem;
            }
            return std::make_pair(std::make_unique<ast::List>(std::move(list)), input);
        }
        case TokenType::quote: {
            auto subexpr_res = parse_expr(input);
            if (not subexpr_res) {
                return std::nullopt;
            }
            auto [subexpr, rem] = *std::move(subexpr_res);
            std::vector<std::unique_ptr<ast::Expr>> list;
            list.push_back(std::make_unique<ast::Atom>("quote"));
            list.push_back(std::move(subexpr));
            return std::make_pair(std::make_unique<ast::List>(std::move(list)), rem);
        }
        case TokenType::identifier:
            return std::make_pair(std::make_unique<ast::Atom>(std::string(t.text)), input);
        case TokenType::number:
            // TODO: what if the number doesn't fit
            std::int64_t v;
            std::from_chars(t.text.data(), t.text.data() + t.text.size(), v);
            return std::make_pair(std::make_unique<ast::NumberLiteral>(v), input);
        case TokenType::string:
            return std::make_pair(std::make_unique<ast::StringLiteral>(std::string(t.text)), input);
    }
}

static Result<std::vector<std::unique_ptr<ast::Expr>>> parse_exprs(std::string_view input) {
    std::vector<std::unique_ptr<ast::Expr>> exprs;
    while (true) {
        auto expr_res = parse_expr(input);
        if (not expr_res) {
            return std::make_pair(std::move(exprs), input);
        }
        exprs.push_back(std::move(expr_res->first));
        input = expr_res->second;
    }
}

std::vector<std::unique_ptr<ast::Expr>> run_parser(std::string_view input) {
    auto exprs_res = parse_exprs(input);
    if (not exprs_res) {
        throw std::runtime_error("unknown parsing error");
    }

    auto exprs = *std::move(exprs_res);
    if (not std::ranges::all_of(exprs.second, [](char c) { return std::isspace(c); })) {
        throw std::runtime_error("unexpected text: " + std::string(exprs.second));
    }

    return std::move(exprs).first;
}

#ifndef PARSER_HPP
#define PARSER_HPP

#include <expected>
#include <ostream>
#include <vector>

#include "ast.hpp"

struct ParseError {
    enum {
        end_of_input,
        mismatched_parentheses,
        unexpected_token,
        invalid_digit,
    } what;
    ast::Source where;
};

inline std::ostream& operator<<(std::ostream& os, ParseError const& e) {
    os << "[Error] at " << e.where.line << ':' << e.where.col << ": ";
    switch (e.what) {
        case ParseError::end_of_input:
            return os << "unexpected end of input";
        case ParseError::mismatched_parentheses:
            return os << "mismatched parentheses";
        case ParseError::unexpected_token:
            return os << "unexpected token";
        case ParseError::invalid_digit:
            return os << "invalid digit in number literal";
    }
    std::unreachable();
}

std::expected<std::vector<ast::ExprPtr>, ParseError> run_parser(std::string_view input) noexcept;

#endif

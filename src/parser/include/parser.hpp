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
    } what;
    ast::Source where;
};

inline std::ostream& operator<<(std::ostream& os, ParseError const& e) {
    os << "[Error] at " << e.where.line << ':' << e.where.col << ": ";
    switch (e.what) {
        case ParseError::end_of_input:
            throw std::logic_error("Internal parser error.");
        case ParseError::mismatched_parentheses:
            return os << "mismatched parentheses";
    }
}

std::expected<std::vector<ast::ExprPtr>, ParseError> run_parser(std::string_view input);

#endif

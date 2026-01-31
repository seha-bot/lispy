#ifndef PARSER_HPP
#define PARSER_HPP

#include <memory>
#include <string_view>
#include <vector>

#include "ast.hpp"

// TODO: this should be noexcept and should return std::expected because
// the exceptions it throws currently are uninformed and the called doesn't
// know which ones to expect.
std::vector<std::unique_ptr<ast::Expr>> run_parser(std::string_view input);

#endif

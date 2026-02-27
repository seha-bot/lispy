
#include "explorerv2.hpp"

#include <cstddef>
#include <expected>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "todo.hpp"

namespace definitions {

namespace {

std::expected<sast::Constructor, Error> parse_constructor(rast::ExprPtr expr) noexcept {
    switch (expr->type()) {
        case rast::ExprType::atom: {
            auto name = std::move(static_cast<rast::Atom&>(*expr).name());
            return sast::Constructor(std::move(name), {});
        }
        case rast::ExprType::list:
            todo();
        case rast::ExprType::number:
        case rast::ExprType::string:
            todo();
    }

    std::unreachable();
}
std::expected<sast::Type, Error> parse_type(rast::ExprPtr expr) noexcept {
    switch (expr->type()) {
        case rast::ExprType::atom:
            return sast::Type(std::move(static_cast<rast::Atom&>(*expr).name()), {});
        case rast::ExprType::list: {
            auto list = std::move(static_cast<rast::List&>(*expr).elements());
            if (list.size() < 2) {
                todo();
            }
            if (not list[0]->is_atom()) {
                todo();
            }
            auto name = std::move(static_cast<rast::Atom&>(*list[0]).name());
            std::vector<std::unique_ptr<sast::Type>> arguments;
            for (std::size_t i = 1; i < list.size(); ++i) {
                auto argument = parse_type(std::move(list[i]));
                if (not argument) {
                    return std::unexpected(argument.error());
                }
                arguments.push_back(std::make_unique<sast::Type>(*std::move(argument)));
            }
            return sast::Type(std::move(name), std::move(arguments));
        }
        case rast::ExprType::number:
        case rast::ExprType::string:
            todo();
    }

    std::unreachable();
}

std::expected<sast::ExprPtr, Error> parse_expr(rast::ExprPtr expr) noexcept;

// std::expected<sast::ExprPtr, Error> parse_lambda_expr(std::vector<rast::ExprPtr> list) noexcept { todo(); }

std::expected<sast::Pattern, Error> parse_pattern_expr(rast::ExprPtr expr) noexcept {
    if (not expr->is_atom()) {
        todo();
    }
    auto constructor_name = std::move(static_cast<rast::Atom&>(*expr).name());
    return sast::Pattern(std::move(constructor_name), {});
}

std::expected<sast::Case, Error> parse_case_expr(std::vector<rast::ExprPtr> list) noexcept {
    if (list.size() < 4 or list.size() % 2 != 0) {
        todo();
    }
    auto expr = parse_expr(std::move(list[1]));
    if (not expr) {
        return std::unexpected(expr.error());
    }
    std::vector<std::pair<sast::Pattern, sast::ExprPtr>> cases;
    for (std::size_t i = 2; i < list.size(); i += 2) {
        auto pattern = parse_pattern_expr(std::move(list[i]));
        if (not pattern) {
            return std::unexpected(pattern.error());
        }
        auto result = parse_expr(std::move(list[i + 1]));
        if (not result) {
            return std::unexpected(result.error());
        }
        cases.emplace_back(*std::move(pattern), *std::move(result));
    }
    return sast::Case(*std::move(expr), std::move(cases));
}

std::expected<sast::ExprPtr, Error> parse_expr(rast::ExprPtr expr) noexcept {
    switch (expr->type()) {
        case rast::ExprType::atom:
            return sast::ExprPtr(static_cast<rast::Atom *>(expr.release()));
        case rast::ExprType::list: {
            auto list = std::move(static_cast<rast::List&>(*expr).elements());
            if (list.empty()) {
                todo();
            }
            switch (list[0]->type()) {
                case rast::ExprType::atom: {
                    auto& callee_or_key = static_cast<rast::Atom&>(*list[0]);
                    if (callee_or_key.name() == "lambda") {
                        // return parse_lambda_expr(std::move(list));
                        todo();
                    } else if (callee_or_key.name() == "case") {
                        auto case_res = parse_case_expr(std::move(list));
                        if (not case_res) {
                            return std::unexpected(case_res.error());
                        }
                        return std::make_unique<sast::Case>(*std::move(case_res));
                    } else {
                        [[fallthrough]];
                    }
                }
                case rast::ExprType::list: {
                    auto callee = parse_expr(std::move(list[0]));
                    if (not callee) {
                        return std::unexpected(callee.error());
                    }
                    std::vector<sast::ExprPtr> arguments;
                    for (std::size_t i = 1; i < list.size(); ++i) {
                        auto argument = parse_expr(std::move(list[i]));
                        if (not argument) {
                            return std::unexpected(argument.error());
                        }
                        arguments.push_back(*std::move(argument));
                    }
                    return std::make_unique<sast::Call>(*std::move(callee), std::move(arguments));
                }
                case rast::ExprType::number:
                case rast::ExprType::string:
                    todo();
            }

            std::unreachable();
        }
        case rast::ExprType::number:
            return sast::ExprPtr(static_cast<rast::Number *>(expr.release()));
        case rast::ExprType::string:
            return sast::ExprPtr(static_cast<rast::String *>(expr.release()));
    }

    std::unreachable();
}

std::expected<sast::Definition, Error> parse_definition(rast::ExprPtr expr) noexcept {
    if (not expr->is_list()) {
        todo();
    }
    auto definition = std::move(static_cast<rast::List&>(*expr).elements());
    if (definition.empty()) {
        todo();
    }
    if (not definition[0]->is_atom()) {
        todo();
    }
    auto& key = static_cast<rast::Atom&>(*definition[0]);

    if (key.name() == "type") {
        // (type ,name ,ctor ,ctors...)
        if (definition.size() < 3) {
            todo();
        }
        if (not definition[1]->is_atom()) {
            todo();
        }
        auto type_name = std::move(static_cast<rast::Atom&>(*definition[1]).name());
        std::vector<sast::Constructor> constructors;
        for (std::size_t i = 2; i < definition.size(); ++i) {
            auto constructor = parse_constructor(std::move(definition[i]));
            if (not constructor) {
                return std::unexpected(constructor.error());
            }
            constructors.push_back(*std::move(constructor));
        }
        return sast::TypeDefinition(std::move(type_name), std::move(constructors));
    } else if (key.name() == "def") {
        // (def type ,type ,name ,args... ,body)
        // (def ,name ,args... ,body)
        if (definition.size() < 2) {
            todo();
        }
        if (not definition[1]->is_atom()) {
            todo();
        }
        auto& name_or_type_key = static_cast<rast::Atom&>(*definition[1]).name();
        if (name_or_type_key == "type") {
            if (definition.size() < 5) {
                todo();
            }
            auto type = parse_type(std::move(definition[2]));
            if (not type) {
                return std::unexpected(type.error());
            }
            if (not definition[3]->is_atom()) {
                todo();
            }
            auto name = std::move(static_cast<rast::Atom&>(*definition[3]).name());
            std::vector<std::string> args;
            for (std::size_t i = 4; i < definition.size() - 1; ++i) {
                if (not definition[i]->is_atom()) {
                    todo();
                }
                args.push_back(std::move(static_cast<rast::Atom&>(*definition[i]).name()));
            }
            auto body = parse_expr(std::move(definition.back()));
            if (not body) {
                return std::unexpected(body.error());
            }
            return sast::FunctionDefinition(*std::move(type), std::move(name), std::move(args), *std::move(body));
        } else {
            todo();
        }
    } else {
        todo();
    }
}

}  // namespace

std::expected<std::vector<sast::Definition>, Error> discover(std::vector<rast::ExprPtr> ast) {
    std::vector<sast::Definition> defs;
    for (auto& expr : ast) {
        auto def = parse_definition(std::move(expr));
        if (not def) {
            return std::unexpected(def.error());
        }
        defs.push_back(*std::move(def));
    }
    return defs;
}

}  // namespace definitions

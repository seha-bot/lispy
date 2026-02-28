
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

std::expected<tast::Type, Error> parse_type_signature(rast::ExprPtr expr) noexcept {
    switch (expr->type()) {
        case rast::ExprType::atom:
            return tast::Type(std::move(static_cast<rast::Atom&>(*expr).name()), {});
        case rast::ExprType::list: {
            auto list = std::move(static_cast<rast::List&>(*expr).elements());
            if (list.size() < 2) {
                todo();
            }
            if (not list[0]->is_atom()) {
                todo();
            }
            auto name = std::move(static_cast<rast::Atom&>(*list[0]).name());
            std::vector<std::unique_ptr<tast::Type>> arguments;
            for (std::size_t i = 1; i < list.size(); ++i) {
                auto argument = parse_type_signature(std::move(list[i]));
                if (not argument) {
                    return std::unexpected(argument.error());
                }
                arguments.push_back(std::make_unique<tast::Type>(*std::move(argument)));
            }
            return tast::Type(std::move(name), std::move(arguments));
        }
        case rast::ExprType::number:
        case rast::ExprType::string:
            todo();
    }

    std::unreachable();
}

std::expected<sast::Pattern, Error> parse_pattern(rast::ExprPtr expr) noexcept {
    if (not expr->is_atom()) {
        todo();
    }
    auto constructor_name = std::move(static_cast<rast::Atom&>(*expr).name());
    return sast::Pattern(std::move(constructor_name), {});
}

std::expected<sast::Expr, Error> parse_expr(rast::ExprPtr expr) noexcept;

std::expected<sast::Case, Error> parse_case(std::vector<rast::ExprPtr> list) noexcept {
    if (list.size() < 4 or list.size() % 2 != 0) {
        todo();
    }
    auto expr = parse_expr(std::move(list[1]));
    if (not expr) {
        return std::unexpected(expr.error());
    }
    std::vector<std::pair<sast::Pattern, sast::ExprPtr>> cases;
    for (std::size_t i = 2; i < list.size(); i += 2) {
        auto pattern = parse_pattern(std::move(list[i]));
        if (not pattern) {
            return std::unexpected(pattern.error());
        }
        auto result = parse_expr(std::move(list[i + 1]));
        if (not result) {
            return std::unexpected(result.error());
        }
        cases.push_back({*std::move(pattern), std::make_unique<sast::Expr>(*std::move(result))});
    }
    return sast::Case(std::make_unique<sast::Expr>(*std::move(expr)), std::move(cases));
}

std::expected<sast::TypeDefinition, Error> parse_type_definition(std::vector<rast::ExprPtr> list) noexcept {
    // (type ,name ,ctor ,ctors...)
    if (list.size() < 3) {
        todo();
    }
    if (not list[1]->is_atom()) {
        todo();
    }
    auto type_name = std::move(static_cast<rast::Atom&>(*list[1]).name());
    std::vector<sast::Constructor> constructors;
    for (std::size_t i = 2; i < list.size(); ++i) {
        auto constructor = parse_constructor(std::move(list[i]));
        if (not constructor) {
            return std::unexpected(constructor.error());
        }
        constructors.push_back(*std::move(constructor));
    }
    return sast::TypeDefinition(std::move(type_name), std::move(constructors));
}

std::expected<sast::ValueDefinition, Error> parse_value_definition(std::vector<rast::ExprPtr> list) noexcept {
    // (def type ,type ,name ,args... ,body)
    // (def ,name ,args... ,body)
    if (list.size() < 2) {
        todo();
    }
    if (not list[1]->is_atom()) {
        todo();
    }
    auto& name_or_type_key = static_cast<rast::Atom&>(*list[1]).name();
    if (name_or_type_key == "type") {
        if (list.size() < 5) {
            todo();
        }
        auto type_signature = parse_type_signature(std::move(list[2]));
        if (not type_signature) {
            return std::unexpected(type_signature.error());
        }
        if (not list[3]->is_atom()) {
            todo();
        }
        auto name = std::move(static_cast<rast::Atom&>(*list[3]).name());

        sast::ExprPtr body;
        {
            auto res = parse_expr(std::move(list.back()));
            if (not res) {
                return std::unexpected(res.error());
            }
            body = std::make_unique<sast::Expr>(*std::move(res));
        }

        for (std::size_t i = list.size() - 2; i >= 4; --i) {
            if (not list[i]->is_atom()) {
                todo();
            }
            auto parameter = std::move(static_cast<rast::Atom&>(*list[i]).name());
            body = std::make_unique<sast::Expr>(sast::Lambda(std::nullopt, std::move(parameter), std::move(body)));
        }
        return sast::ValueDefinition(*std::move(type_signature), std::move(name), std::move(body));
    } else {
        todo();
    }
}

std::expected<sast::Expr, Error> parse_expr(rast::ExprPtr expr) noexcept {
    switch (expr->type()) {
        case rast::ExprType::atom:
            return sast::Atom(std::move(static_cast<rast::Atom&>(*expr).name()));
        case rast::ExprType::list: {
            auto list = std::move(static_cast<rast::List&>(*expr).elements());
            if (list.empty()) {
                todo();
            }
            switch (list[0]->type()) {
                case rast::ExprType::atom: {
                    auto& key = static_cast<rast::Atom&>(*list[0]);

                    if (key.name() == "lambda") {
                        todo();
                    } else if (key.name() == "case") {
                        return parse_case(std::move(list));
                    } else if (key.name() == "type") {
                        return parse_type_definition(std::move(list));
                    } else if (key.name() == "def") {
                        return parse_value_definition(std::move(list));
                    } else {
                        [[fallthrough]];
                    }
                }
                case rast::ExprType::list: {
                    // TODO: factor out to a parse_call function
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
                        arguments.push_back(std::make_unique<sast::Expr>(*std::move(argument)));
                    }
                    return sast::Call(std::make_unique<sast::Expr>(*std::move(callee)), std::move(arguments));
                }
                case rast::ExprType::number:
                case rast::ExprType::string:
                    todo();
            }

            std::unreachable();
        }
        case rast::ExprType::number:
            return sast::Number(static_cast<rast::Number&>(*expr).value());
        case rast::ExprType::string:
            todo();
    }

    std::unreachable();
}

}  // namespace

// TODO: this needs a better name (also the file needs a better name)
std::expected<sast::Block, Error> discover(std::vector<rast::ExprPtr> ast) {
    std::vector<sast::ExprPtr> defs;
    for (auto& expr : ast) {
        auto def = parse_expr(std::move(expr));
        if (not def) {
            return std::unexpected(def.error());
        }
        defs.push_back(std::make_unique<sast::Expr>(*std::move(def)));
    }
    return sast::Block(std::move(defs));
}

}  // namespace definitions

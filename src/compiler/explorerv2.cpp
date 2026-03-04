
#include "explorerv2.hpp"

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "ast.hpp"
#include "todo.hpp"

namespace compiler {

namespace {

// vacant is a cool word

struct EntityStorage {
    ast::EntityId reserve() {
        ast::EntityId id(m_entities.size());
        m_entities.push_back(std::nullopt);
        return id;
    }

    void store(ast::EntityId id, ast::Entity entity) { m_entities[id.id] = std::move(entity); }

    std::expected<std::vector<ast::Entity>, Error> produce() noexcept {
        std::vector<ast::Entity> entities;
        entities.reserve(m_entities.size());
        for (auto& entity : m_entities) {
            if (not entity) {
                todo();
            }
            entities.push_back(*std::move(entity));
        }
        return entities;
    }

private:
    std::vector<std::optional<ast::Entity>> m_entities;
};

struct Scope {
    std::optional<ast::EntityId> lookup(std::string_view name) const {
        auto it = entities.find(std::string(name));
        if (it == entities.end()) {
            if (not parent) {
                return std::nullopt;
            }
            return parent->lookup(name);
        }
        return it->second;
    }

    std::unordered_map<std::string, ast::EntityId> entities;
    Scope const *parent{};
};

///////////////////////////////////////////////////////////////////////////

std::expected<ast::Type, Error> compile_raw_type_signature(ast::Expr) noexcept { return ast::Type("Undefined", {}); }

std::expected<ast::Pattern, Error> compile_pattern(EntityStorage&, Scope const&, ast::Expr) noexcept {
    return ast::Pattern{"Undefined", {}};
}

///////////////////////////////////////////////////////////////////////////

std::expected<ast::Expr, Error> compile_expr(EntityStorage& storage, Scope const& scope, ast::Expr expr) noexcept {
    struct Visitor {
        std::expected<ast::Expr, Error> operator()(ast::Atom atom) {
            auto entity_id = scope.lookup(atom.name());
            if (not entity_id) {
                todo();
            }
            return ast::EntityReference(*entity_id);
        }
        std::expected<ast::Expr, Error> operator()(ast::List list) {
            if (list.empty()) {
                todo();
            }
            if (auto *key = std::get_if<ast::Atom>(&list[0])) {
                auto entity_id = scope.lookup(key->name());
                if (not entity_id) {
                    if (key->name() == "lambda") {
                        if (list.size() < 2) {
                            todo();
                        }
                        if (auto *raw_type_signature = std::get_if<ast::List>(&list[1])) {
                            if (list.size() != 4) {
                                todo();
                            }
                            if (raw_type_signature->size() != 2) {
                                todo();
                            }
                            if (auto *type_key = std::get_if<ast::Atom>(&(*raw_type_signature)[0]);
                                not type_key or type_key->name() != "type") {
                                todo();
                            }
                            auto type_signature = compile_raw_type_signature(std::move((*raw_type_signature)[1]));
                            if (not type_signature) {
                                return std::unexpected(type_signature.error());
                            }

                            auto *parameter_name = std::get_if<ast::Atom>(&list[2]);
                            if (not parameter_name) {
                                todo();
                            }

                            auto parameter_id = storage.reserve();
                            Scope const lambda_scope{{{parameter_name->name(), parameter_id}}, &scope};

                            storage.store(parameter_id, ast::LambdaParameter{std::move(parameter_name->name())});

                            auto body = compile_expr(storage, lambda_scope, std::move(list[3]));
                            if (not body) {
                                return std::unexpected(body.error());
                            }

                            return ast::Lambda{*std::move(type_signature), parameter_id,
                                               std::make_unique<ast::Expr>(*std::move(body))};
                        }

                        if (list.size() != 3) {
                            todo();
                        }
                        auto *parameter_name = std::get_if<ast::Atom>(&list[1]);
                        if (not parameter_name) {
                            todo();
                        }

                        auto parameter_id = storage.reserve();
                        Scope const lambda_scope{{{parameter_name->name(), parameter_id}}, &scope};

                        storage.store(parameter_id, ast::LambdaParameter{std::move(parameter_name->name())});

                        auto body = compile_expr(storage, lambda_scope, std::move(list[2]));
                        if (not body) {
                            return std::unexpected(body.error());
                        }

                        return ast::Lambda{std::nullopt, parameter_id, std::make_unique<ast::Expr>(*std::move(body))};
                    } else if (key->name() == "case") {
                        if (list.size() < 4 or list.size() % 2 != 0) {
                            todo();
                        }
                        auto scrutinee = compile_expr(storage, scope, std::move(list[1]));
                        if (not scrutinee) {
                            return std::unexpected(scrutinee.error());
                        }
                        std::vector<std::pair<ast::Pattern, ast::Expr>> cases;
                        for (std::size_t i = 2; i < list.size(); i += 2) {
                            auto pattern = compile_pattern(storage, scope, std::move(list[i]));
                            if (not pattern) {
                                return std::unexpected(pattern.error());
                            }
                            auto expression = compile_expr(storage, scope, std::move(list[i + 1]));
                            if (not expression) {
                                return std::unexpected(expression.error());
                            }
                            cases.push_back({*std::move(pattern), *std::move(expression)});
                        }

                        return ast::Case{std::make_unique<ast::Expr>(*std::move(scrutinee)), std::move(cases)};
                    } else {
                        todo();
                    }
                }
            }

            auto callee = compile_expr(storage, scope, std::move(list[0]));
            if (not callee) {
                return callee;
            }

            // TODO: reserve
            std::vector<ast::Expr> arguments;
            for (std::size_t i = 1; i < list.size(); ++i) {
                auto subexpr = compile_expr(storage, scope, std::move(list[i]));
                if (not subexpr) {
                    return subexpr;
                }
                arguments.push_back(*std::move(subexpr));
            }

            return ast::Call{std::make_unique<ast::Expr>(*std::move(callee)), std::move(arguments)};
        }
        std::expected<ast::Expr, Error> operator()(ast::Number number) { return number; }
        std::expected<ast::Expr, Error> operator()(ast::EntityReference) { todo(); }
        std::expected<ast::Expr, Error> operator()(ast::Call) { todo(); }
        std::expected<ast::Expr, Error> operator()(ast::Case) { todo(); }
        std::expected<ast::Expr, Error> operator()(ast::Lambda) { todo(); }

        EntityStorage& storage;
        Scope const& scope;
    };

    return std::visit(Visitor{storage, scope}, std::move(expr));
}

///////////////////////////////////////////////////////////////////////////

std::expected<ast::ShallowEntity, Error> compile_raw_module_entity(ast::Expr expr) noexcept {
    auto *list_opt = std::get_if<ast::List>(&expr);
    if (not list_opt or list_opt->size() < 2) {
        todo();
    }
    auto& list = *list_opt;

    auto *key = std::get_if<ast::Atom>(&list[0]);
    if (not key) {
        todo();
    }
    if (key->name() == "def") {
        if (list.size() < 2) {
            todo();
        }
        if (auto *raw_type_signature = std::get_if<ast::List>(&list[1])) {
            if (list.size() != 4) {
                todo();
            }
            if (raw_type_signature->size() != 2) {
                todo();
            }
            if (auto *type_key = std::get_if<ast::Atom>(&(*raw_type_signature)[0]);
                not type_key or type_key->name() != "type") {
                todo();
            }
            auto type_signature = compile_raw_type_signature(std::move((*raw_type_signature)[1]));
            if (not type_signature) {
                return std::unexpected(type_signature.error());
            }

            auto *name = std::get_if<ast::Atom>(&list[2]);
            if (not name) {
                todo();
            }

            return ast::ShallowValueDefinition{*std::move(type_signature), std::move(name->name()), std::move(list[3])};
        }

        if (list.size() != 3) {
            todo();
        }
        auto *name = std::get_if<ast::Atom>(&list[1]);
        if (not name) {
            todo();
        }
        return ast::ShallowValueDefinition{std::nullopt, std::move(name->name()), std::move(list[2])};
    } else if (key->name() == "type") {
        todo();
    } else if (key->name() == "module") {
        if (list.size() < 2) {
            todo();
        }
        auto *name = std::get_if<ast::Atom>(&list[1]);
        if (not name) {
            todo();
        }
        return ast::ShallowModule{std::move(name->name()), std::move(list).drop(2)};
    } else if (key->name() == "defmacro") {
        todo();
    } else {
        todo();
    }
}

///////////////////////////////////////////////////////////////////////////

std::expected<ast::Entity, Error> compile_shallow_entity(EntityStorage& storage, Scope const& scope,
                                                         ast::ShallowEntity entity) noexcept;

std::expected<ast::ValueDefinition, Error> compile_shallow_entity(EntityStorage& storage, Scope const& scope,
                                                                  ast::ShallowValueDefinition value) {
    auto expr = compile_expr(storage, scope, std::move(value.raw_value));
    if (not expr) {
        return std::unexpected(expr.error());
    }
    return ast::ValueDefinition{std::move(value.type_signature), std::move(value.name), *std::move(expr)};
}

std::expected<ast::TypeDefinition, Error> compile_shallow_entity(EntityStorage&, Scope const&, ast::TypeDefinition) {
    todo();
}

std::expected<ast::MacroDefinition, Error> compile_shallow_entity(EntityStorage&, Scope const&,
                                                                  ast::ShallowMacroDefinition) {
    todo();
}

std::expected<ast::Module, Error> compile_shallow_entity(EntityStorage& storage, Scope const& scope,
                                                         ast::ShallowModule module) {
    auto *list_opt = std::get_if<ast::List>(&module.rest);
    if (not list_opt) {
        todo();
    }
    auto& list = *list_opt;

    std::vector<ast::ShallowEntity> shallow_entities;
    std::unordered_map<std::string, ast::EntityId> scope_entities;
    std::vector<ast::EntityId> module_entities;
    for (std::size_t i = 0; i < list.size(); ++i) {
        auto shallow_entity = compile_raw_module_entity(std::move(list[i]));
        if (not shallow_entity) {
            return std::unexpected(shallow_entity.error());
        }
        auto name = std::visit([](auto& entity) { return static_cast<std::string>(entity.name); }, *shallow_entity);
        shallow_entities.push_back(*std::move(shallow_entity));
        auto id = storage.reserve();
        auto [it, did_insert] = scope_entities.try_emplace(std::move(name), id);
        if (not did_insert) {
            todo();
        }
        module_entities.push_back(id);
    }
    Scope const module_scope{std::move(scope_entities), &scope};

    // TODO: reserve
    for (std::size_t i = 0; i < shallow_entities.size(); ++i) {
        auto res = compile_shallow_entity(storage, module_scope, std::move(shallow_entities[i]));
        if (not res) {
            return std::unexpected(res.error());
        }
        storage.store(module_entities[i], *std::move(res));
    }

    return ast::Module{std::move(module.name), std::move(module_entities)};
}

std::expected<ast::Entity, Error> compile_shallow_entity(EntityStorage& storage, Scope const& scope,
                                                         ast::ShallowEntity entity) noexcept {
    return std::visit(
        [&storage, &scope](auto unwrapped_entity) -> std::expected<ast::Entity, Error> {
            auto res = compile_shallow_entity(storage, scope, std::move(unwrapped_entity));
            if (not res) {
                return std::unexpected(res.error());
            }
            return *std::move(res);
        },
        std::move(entity));
}

}  // namespace

std::expected<Result, Error> lower_ast(std::string filename, std::vector<ast::Expr> ast) noexcept {
    EntityStorage storage;
    Scope const scope;
    auto shallow_module = ast::ShallowModule{std::move(filename), ast::List(std::move(ast), ast::Source{})};
    auto module = compile_shallow_entity(storage, scope, std::move(shallow_module));
    if (not module) {
        return std::unexpected(module.error());
    }

    auto entities = storage.produce();
    if (not entities) {
        return std::unexpected(entities.error());
    }

    return Result{*std::move(module), *std::move(entities)};
}

}  // namespace compiler

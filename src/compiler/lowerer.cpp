
#include "lowerer.hpp"

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

// TODO: does this need to be in an anonymous namespace?
namespace {

// TODO: rename compile_ to lower_

// vacant is a cool word

// TODO: create a context class

struct EntityStorage {
    ast::EntityId reserve() {
        ast::EntityId id(m_entities.size());
        m_entities.push_back(std::nullopt);
        return id;
    }

    void store(ast::EntityId id, ast::Entity entity) { m_entities[id.id] = std::move(entity); }

    std::optional<std::string_view> name_of(ast::EntityId id) const {
        return m_entities[id.id].transform([](auto& entity) {
            return std::visit([](auto& e) -> std::string_view { return e.name; }, entity);
        });
    }

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

    ast::LabelId get_label(std::string name) {
        auto id = ast::LabelId{m_labels.size()};
        ast::Label label{std::move(name)};

        auto [iter, did_insert] = m_labels.insert({std::move(label), id});
        if (did_insert) {
            iter->second = id;
        }
        return iter->second;
    }

    ast::TypeFormDefinition *get_if_type_form_definition(ast::EntityId id) {
        auto& entity_opt = m_entities[id.id];
        if (not entity_opt) {
            // TODO: i think this triggers when you try constructing a recursive type
            // which can never be constructed.
            todo();
        }
        return std::get_if<ast::TypeFormDefinition>(&*entity_opt);
    }

private:
    std::vector<std::optional<ast::Entity>> m_entities;
    using Hasher = decltype([](ast::Label const& l) { return std::hash<std::string>{}(l.name); });
    std::unordered_map<ast::Label, ast::LabelId, Hasher> m_labels;
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

namespace raw {

std::expected<ast::Kind, Error> compile_raw_kind(ast::RawExpr expr) noexcept {
    struct Visitor {
        std::expected<ast::Kind, Error> operator()(ast::Atom atom) {
            if (atom.name() == "*") {
                return ast::Kind{};
            }
            todo();
        }
        std::expected<ast::Kind, Error> operator()(ast::List list) {
            if (list.size() != 3) {
                todo();
            }
            if (auto *to = std::get_if<ast::Atom>(&list[0]); not to or to->name() != "to") {
                todo();
            }
            auto from = compile_raw_kind(std::move(list[1]));
            if (not from) {
                return std::unexpected(from.error());
            }
            auto to = compile_raw_kind(std::move(list[1]));
            if (not to) {
                return std::unexpected(to.error());
            }

            return ast::Kind{{{std::make_unique<ast::Kind>(*std::move(from)),
                               std::make_unique<ast::Kind>(*std::move(to))}}};
        }
        std::expected<ast::Kind, Error> operator()(ast::Number) { todo(); }
    };
    return std::visit(Visitor{}, std::move(expr));
}

std::expected<ast::TypeId, Error> compile_raw_type(TypeStorage& ts, EntityStorage& storage,
                                                   Scope const& scope, ast::RawExpr expr) noexcept {
    if (auto *atom = std::get_if<ast::Atom>(&expr)) {
        auto type_entity_id = scope.lookup(atom->name());
        if (not type_entity_id) {
            todo();
        }
        // TODO: make sure this actually is a type

        // auto *type_entity = storage.get_if_type_form_definition(*type_entity_id);
        // if (not type_entity) {
        //     todo();
        // }
        return ts.store(ast::TypeReference{*type_entity_id});
    }

    auto *list_opt = std::get_if<ast::List>(&expr);
    if (not list_opt or list_opt->empty()) {
        todo();
    }
    auto& list = *list_opt;

    auto *key = std::get_if<ast::Atom>(&list[0]);
    if (not key) {
        todo();
    }

    if (key->name() == "tuple") {
        std::vector<ast::TypeId> elements;
        for (std::size_t i = 1; i < list.size(); ++i) {
            auto type = raw::compile_raw_type(ts, storage, scope, std::move(list[i]));
            if (not type) {
                return std::unexpected(type.error());
            }
            elements.push_back(*std::move(type));
        }
        return ts.store(ast::TypeTuple{std::move(elements)});
    } else if (key->name() == "variant") {
        std::vector<std::pair<ast::LabelId, std::optional<ast::TypeId>>> variants;
        for (std::size_t i = 1; i < list.size(); ++i) {
            // TODO: no need for visitor here cuz you only ever need to account for atom and list
            // variants.
            struct Visitor {
                std::expected<std::pair<ast::LabelId, std::optional<ast::TypeId>>, Error>
                operator()(ast::Atom atom) {
                    // TODO: what if atom isn't a label?
                    return {{storage.get_label(std::move(atom.name())), std::nullopt}};
                }
                std::expected<std::pair<ast::LabelId, std::optional<ast::TypeId>>, Error>
                operator()(ast::List list) {
                    if (list.size() != 2) {
                        todo();
                    }
                    // TODO: what if this isn't a label?
                    auto *label = std::get_if<ast::Atom>(&list[0]);
                    if (not label) {
                        todo();
                    }
                    auto type = raw::compile_raw_type(ts, storage, scope, std::move(list[1]));
                    if (not type) {
                        return std::unexpected(type.error());
                    }
                    return {{storage.get_label(std::move(label->name())), *std::move(type)}};
                }
                std::expected<std::pair<ast::LabelId, std::optional<ast::TypeId>>, Error>
                operator()(ast::Number) {
                    todo();
                }

                TypeStorage& ts;
                EntityStorage& storage;
                Scope const& scope;
            };

            auto variant = std::visit(Visitor{ts, storage, scope}, std::move(list[i]));
            if (not variant) {
                return std::unexpected(variant.error());
            }
            variants.push_back(*std::move(variant));
        }
        return ts.store(ast::TypeVariant{std::move(variants)});
    } else if (key->name() == "to") {
        if (list.size() != 3) {
            todo();
        }
        auto from = compile_raw_type(ts, storage, scope, std::move(list[1]));
        if (not from) {
            return std::unexpected(from.error());
        }
        auto to = compile_raw_type(ts, storage, scope, std::move(list[2]));
        if (not to) {
            return std::unexpected(to.error());
        }
        return ts.store(ast::TypeArrow{*from, *to});
    } else if (key->name() == "tt-lambda") {
        todo();
        // if (list.size() != 3) {
        //     todo();
        // }

        // if (auto *kinded_parameter = std::get_if<ast::List>(&list[2])) {
        //     auto *parameter_name = std::get_if<ast::Atom>(&(*kinded_parameter)[0]);
        //     if (not parameter_name) {
        //         todo();
        //     }

        //     auto kind = compile_raw_kind(std::move((*kinded_parameter)[1]));
        //     if (not kind) {
        //         return std::unexpected(kind.error());
        //     }

        //     auto parameter_id = storage.reserve();
        //     Scope const lambda_scope{{{parameter_name->name(), parameter_id}}, &scope};

        //     storage.store(parameter_id,
        //     ast::TTLambda::Parameter{std::move(parameter_name->name())});

        //     auto body = compile_raw_type(storage, lambda_scope, std::move(list[2]));
        //     if (not body) {
        //         return std::unexpected(body.error());
        //     }
        //     return ast::TTLambda{parameter_id, *std::move(kind),
        //     std::make_unique<ast::Type>(*std::move(body))};
        // }

        // todo();
    } else {
        todo();
        // if (list.size() < 2) {
        //     todo();
        // }
        // auto function = compile_raw_type(storage, scope, std::move(list[0]));
        // if (not function) {
        //     return std::unexpected(function.error());
        // }
        // std::vector<ast::Type> arguments;
        // for (std::size_t i = 1; i < list.size(); ++i) {
        //     auto argument = compile_raw_type(storage, scope, std::move(list[i]));
        //     if (not argument) {
        //         return std::unexpected(argument.error());
        //     }
        //     arguments.push_back(*std::move(argument));
        // }
        // return ast::TypeApplication{std::make_unique<ast::Type>(*std::move(function)),
        // std::move(arguments)};
    }
}

std::expected<ast::Expr, Error> compile_raw_expr(TypeStorage& ts, EntityStorage& storage,
                                                 Scope const& scope, ast::RawExpr expr) noexcept;

std::expected<ast::Case::Alternative, Error> compile_raw_pattern(TypeStorage& ts,
                                                                 EntityStorage& storage,
                                                                 Scope const& scope,
                                                                 ast::RawExpr pattern,
                                                                 ast::RawExpr expr) noexcept {
    struct Visitor {
        std::expected<ast::Case::Alternative, Error> operator()(ast::Atom atom) {
            if (atom.name()[0] != ':') {
                todo();
            }
            auto e = compile_raw_expr(ts, storage, scope, std::move(expr));
            if (not e) {
                return std::unexpected(e.error());
            }
            return {{{storage.get_label(std::move(atom.name())), std::nullopt}, *std::move(e)}};
        }
        std::expected<ast::Case::Alternative, Error> operator()(ast::List list) {
            if (list.size() != 2) {
                todo();
            }
            auto *atom = std::get_if<ast::Atom>(&list[0]);
            if (not atom) {
                todo();
            }
            if (atom->name()[0] != ':') {
                todo();
            }

            auto *binding_name = std::get_if<ast::Atom>(&list[1]);
            if (not binding_name) {
                todo();
            }

            auto binding_id = storage.reserve();
            Scope const pattern_scope{{{binding_name->name(), binding_id}}, &scope};
            storage.store(binding_id, ast::PatternBinding{std::move(binding_name->name())});

            auto e = compile_raw_expr(ts, storage, pattern_scope, std::move(expr));
            if (not e) {
                return std::unexpected(e.error());
            }
            return {{{storage.get_label(std::move(atom->name())), binding_id}, *std::move(e)}};
        }
        std::expected<ast::Case::Alternative, Error> operator()(ast::Number) { todo(); }

        TypeStorage& ts;
        EntityStorage& storage;
        Scope const& scope;
        ast::RawExpr expr;
    };

    return std::visit(Visitor{ts, storage, scope, std::move(expr)}, std::move(pattern));
}

std::expected<ast::Expr, Error> compile_raw_expr(TypeStorage& ts, EntityStorage& storage,
                                                 Scope const& scope, ast::RawExpr expr) noexcept {
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
                        if (auto *typed_parameter = std::get_if<ast::List>(&list[1])) {
                            if (list.size() != 3) {
                                todo();
                            }
                            if (typed_parameter->size() != 2) {
                                todo();
                            }

                            auto *parameter_name = std::get_if<ast::Atom>(&(*typed_parameter)[0]);
                            if (not parameter_name) {
                                todo();
                            }

                            auto parameter_type = compile_raw_type(
                                ts, storage, scope, std::move((*typed_parameter)[1]));
                            if (not parameter_type) {
                                return std::unexpected(parameter_type.error());
                            }

                            auto parameter_id = storage.reserve();
                            Scope const lambda_scope{{{parameter_name->name(), parameter_id}},
                                                     &scope};

                            storage.store(parameter_id,
                                          ast::Lambda::Parameter{std::move(parameter_name->name()),
                                                                 *std::move(parameter_type)});

                            auto body =
                                compile_raw_expr(ts, storage, lambda_scope, std::move(list[2]));
                            if (not body) {
                                return std::unexpected(body.error());
                            }

                            return ast::Lambda{parameter_id,
                                               std::make_unique<ast::Expr>(*std::move(body))};
                        } else if (auto *untyped_parameter = std::get_if<ast::Atom>(&list[1])) {
                            auto parameter_id = storage.reserve();
                            Scope const lambda_scope{{{untyped_parameter->name(), parameter_id}},
                                                     &scope};

                            storage.store(parameter_id,
                                          ast::Lambda::Parameter{
                                              std::move(untyped_parameter->name()), std::nullopt});

                            auto body =
                                compile_raw_expr(ts, storage, lambda_scope, std::move(list[2]));
                            if (not body) {
                                return std::unexpected(body.error());
                            }

                            return ast::Lambda{parameter_id,
                                               std::make_unique<ast::Expr>(*std::move(body))};
                        }
                        todo();
                    } else if (key->name() == "tv-lambda") {
                        if (list.size() != 4) {
                            todo();
                        }
                        if (auto *raw_kind_signature = std::get_if<ast::List>(&list[1])) {
                            if (raw_kind_signature->size() != 2) {
                                todo();
                            }
                            if (auto *kind_key = std::get_if<ast::Atom>(&(*raw_kind_signature)[0]);
                                not kind_key or kind_key->name() != "kind") {
                                todo();
                            }
                            auto kind_signature =
                                compile_raw_kind(std::move((*raw_kind_signature)[1]));
                            if (not kind_signature) {
                                return std::unexpected(kind_signature.error());
                            }

                            auto *parameter_name = std::get_if<ast::Atom>(&list[2]);
                            if (not parameter_name) {
                                todo();
                            }

                            auto parameter_id = storage.reserve();
                            Scope const quantifier_scope{{{parameter_name->name(), parameter_id}},
                                                         &scope};

                            storage.store(parameter_id, ast::TVLambda::Parameter{
                                                            std::move(parameter_name->name())});

                            auto body =
                                compile_raw_expr(ts, storage, quantifier_scope, std::move(list[3]));
                            if (not body) {
                                return std::unexpected(body.error());
                            }

                            return ast::TVLambda{parameter_id, *std::move(kind_signature),
                                                 std::make_unique<ast::Expr>(*std::move(body))};
                        }
                        todo();
                    } else if (key->name() == "case") {
                        if (list.size() < 4 or list.size() % 2 != 0) {
                            todo();
                        }
                        auto scrutinee = compile_raw_expr(ts, storage, scope, std::move(list[1]));
                        if (not scrutinee) {
                            return std::unexpected(scrutinee.error());
                        }
                        std::vector<ast::Case::Alternative> alternatives;
                        for (std::size_t i = 2; i < list.size(); i += 2) {
                            auto pattern_and_expr = compile_raw_pattern(
                                ts, storage, scope, std::move(list[i]), std::move(list[i + 1]));
                            if (not pattern_and_expr) {
                                return std::unexpected(pattern_and_expr.error());
                            }
                            alternatives.push_back(*std::move(pattern_and_expr));
                        }

                        return ast::Case{std::make_unique<ast::Expr>(*std::move(scrutinee)),
                                         std::move(alternatives)};
                    } else if (key->name()[0] == ':') {
                        auto label = storage.get_label(std::move(key->name()));
                        if (list.size() < 2) {
                            todo();
                        }
                        auto type = compile_raw_type(ts, storage, scope, std::move(list[1]));
                        if (not type) {
                            return std::unexpected(type.error());
                        }
                        std::optional<ast::ExprPtr> argument;
                        if (list.size() == 3) {
                            auto arg = compile_raw_expr(ts, storage, scope, std::move(list[2]));
                            if (not arg) {
                                return std::unexpected(arg.error());
                            }
                            argument = std::make_unique<ast::Expr>(*std::move(arg));
                        } else if (list.size() > 3) {
                            todo();
                        }
                        return ast::LabelCall{label, *type, std::move(argument)};
                    } else {
                        todo();
                    }
                }
            }

            auto callee = compile_raw_expr(ts, storage, scope, std::move(list[0]));
            if (not callee) {
                return callee;
            }

            // TODO: reserve
            std::vector<ast::Expr> arguments;
            for (std::size_t i = 1; i < list.size(); ++i) {
                auto subexpr = compile_raw_expr(ts, storage, scope, std::move(list[i]));
                if (not subexpr) {
                    return subexpr;
                }
                arguments.push_back(*std::move(subexpr));
            }

            return ast::Call{std::make_unique<ast::Expr>(*std::move(callee)), std::move(arguments)};
        }
        std::expected<ast::Expr, Error> operator()(ast::Number number) { return number; }

        TypeStorage& ts;
        EntityStorage& storage;
        Scope const& scope;
    };

    return std::visit(Visitor{ts, storage, scope}, std::move(expr));
}

std::expected<ast::ShallowEntity, Error> compile_raw_module_entity(EntityStorage&, Scope const&,
                                                                   ast::RawExpr expr) noexcept {
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
        if (list.size() != 3) {
            todo();
        }
        auto *name = std::get_if<ast::Atom>(&list[1]);
        if (not name) {
            todo();
        }
        return ast::ShallowValueDefinition{std::move(name->name()), std::move(list[2])};
    } else if (key->name() == "form") {
        if (list.size() != 3) {
            todo();
        }
        auto *name = std::get_if<ast::Atom>(&list[1]);
        if (not name) {
            todo();
        }
        return ast::ShallowTypeFormDefinition{std::move(name->name()), std::move(list[2])};
    } else if (key->name() == "dec") {
        if (list.size() != 3) {
            todo();
        }
        auto *name = std::get_if<ast::Atom>(&list[1]);
        if (not name) {
            todo();
        }
        return ast::ShallowValueDeclaration{std::move(name->name()), std::move(list[2])};
    }
    if (key->name() == "module") {
        if (list.size() < 2) {
            todo();
        }
        auto *name = std::get_if<ast::Atom>(&list[1]);
        if (not name) {
            todo();
        }
        return ast::ShallowModuleDefinition{std::move(name->name()), std::move(list).drop(2)};
    } else {
        todo();
    }
}

}  // namespace raw

std::expected<ast::Entity, Error> compile_shallow_entity(TypeStorage& ts, EntityStorage& storage,
                                                         Scope const& scope,
                                                         ast::ShallowEntity entity) noexcept;

namespace shallow {

std::expected<ast::ValueDefinition, Error> compile_shallow_entity(
    TypeStorage& ts, EntityStorage& storage, Scope const& scope,
    ast::ShallowValueDefinition value) {
    auto expr = raw::compile_raw_expr(ts, storage, scope, std::move(value.raw_value));
    if (not expr) {
        return std::unexpected(expr.error());
    }
    return ast::ValueDefinition{std::move(value.name), *std::move(expr)};
}

std::expected<ast::TypeFormDefinition, Error> compile_shallow_entity(
    TypeStorage& ts, EntityStorage& storage, Scope const& scope,
    ast::ShallowTypeFormDefinition shallow_type_form) {
    auto type = raw::compile_raw_type(ts, storage, scope, std::move(shallow_type_form.raw_type));
    if (not type) {
        return std::unexpected(type.error());
    }
    return ast::TypeFormDefinition{std::move(shallow_type_form.name), *std::move(type)};
}

std::expected<ast::ValueDeclaration, Error> compile_shallow_entity(
    TypeStorage& ts, EntityStorage& storage, Scope const& scope,
    ast::ShallowValueDeclaration shallow_value_declaration) {
    auto type_signature = raw::compile_raw_type(
        ts, storage, scope, std::move(shallow_value_declaration.raw_type_signature));
    if (not type_signature) {
        return std::unexpected(type_signature.error());
    }
    return ast::ValueDeclaration{std::move(shallow_value_declaration.name),
                                 *std::move(type_signature)};
}

std::expected<ast::MergedValueDefinition, Error> compile_shallow_entity(
    TypeStorage& ts, EntityStorage& storage, Scope const& scope,
    ast::ShallowMergedValueDefinition shallow_merged_value_definition) {
    auto type_signature = raw::compile_raw_type(
        ts, storage, scope, std::move(shallow_merged_value_definition.raw_type_signature));
    if (not type_signature) {
        return std::unexpected(type_signature.error());
    }

    auto expr = raw::compile_raw_expr(ts, storage, scope,
                                      std::move(shallow_merged_value_definition.raw_value));
    if (not expr) {
        return std::unexpected(expr.error());
    }

    return ast::MergedValueDefinition{std::move(shallow_merged_value_definition.name),
                                      *std::move(type_signature), *std::move(expr)};
}

std::expected<ast::ModuleDefinition, Error> compile_shallow_entity(
    TypeStorage& ts, EntityStorage& storage, Scope const& scope,
    ast::ShallowModuleDefinition module) {
    std::vector<ast::ShallowEntity> shallow_entities;
    std::unordered_map<std::string, ast::EntityId> scope_entities;
    std::vector<ast::EntityId> module_entities;

    for (std::size_t i = 0; i < module.raw_entities.size(); ++i) {
        auto shallow_entity =
            raw::compile_raw_module_entity(storage, scope, std::move(module.raw_entities[i]));
        if (not shallow_entity) {
            return std::unexpected(shallow_entity.error());
        }
        auto name = std::visit([](auto& entity) { return static_cast<std::string>(entity.name); },
                               *shallow_entity);
        if (scope_entities.contains(name)) {
            auto id = scope_entities.at(name);
            auto i = std::ranges::find(module_entities, id) - module_entities.begin();
            auto *decl = std::get_if<ast::ShallowValueDeclaration>(&shallow_entities.at(i));
            auto *def = std::get_if<ast::ShallowValueDefinition>(&*shallow_entity);
            if (decl and def) {
                shallow_entities.at(i) = ast::ShallowMergedValueDefinition{
                    std::move(decl->name), std::move(decl->raw_type_signature),
                    std::move(def->raw_value)};
            } else {
                todo();
            }
        } else {
            shallow_entities.push_back(*std::move(shallow_entity));
            auto id = storage.reserve();
            scope_entities.try_emplace(std::move(name), id);
            module_entities.push_back(id);
        }
    }
    Scope const module_scope{std::move(scope_entities), &scope};

    // TODO: reserve
    for (std::size_t i = 0; i < shallow_entities.size(); ++i) {
        auto res =
            compile_shallow_entity(ts, storage, module_scope, std::move(shallow_entities[i]));
        if (not res) {
            return std::unexpected(res.error());
        }
        storage.store(module_entities[i], *std::move(res));
    }

    return ast::ModuleDefinition{std::move(module.name), std::move(module_entities)};
}

}  // namespace shallow

std::expected<ast::Entity, Error> compile_shallow_entity(TypeStorage& ts, EntityStorage& storage,
                                                         Scope const& scope,
                                                         ast::ShallowEntity entity) noexcept {
    return std::visit(
        [&ts, &storage, &scope](auto unwrapped_entity) -> std::expected<ast::Entity, Error> {
            auto res =
                shallow::compile_shallow_entity(ts, storage, scope, std::move(unwrapped_entity));
            if (not res) {
                return std::unexpected(res.error());
            }
            return *std::move(res);
        },
        std::move(entity));
}

}  // namespace

std::expected<ResolvedAST, Error> lower_ast(std::string filename,
                                            std::vector<ast::RawExpr> ast) noexcept {
    auto ts = std::make_unique<TypeStorage>();
    EntityStorage storage;
    Scope const scope;
    auto shallow_module =
        ast::ShallowModuleDefinition{std::move(filename), ast::List(std::move(ast), ast::Source{})};
    auto module = shallow::compile_shallow_entity(*ts, storage, scope, std::move(shallow_module));
    if (not module) {
        return std::unexpected(module.error());
    }

    auto entities = storage.produce();
    if (not entities) {
        return std::unexpected(entities.error());
    }

    return ResolvedAST{*std::move(module), std::move(ts), *std::move(entities)};
}

}  // namespace compiler

bool ast::TypeId::operator==(TypeId const& that) const { return m_rep->equal(*this, that); }
std::string ast::TypeId::to_string() const {
    return std::to_string(m_rep->representative(*this).m_id);
}

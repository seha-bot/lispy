#ifndef EXPLORER_HPP
#define EXPLORER_HPP

#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "ast.hpp"

struct StaticError {
    std::string msg;
    ast::Expr& expr;

    friend std::ostream& operator<<(std::ostream& os, StaticError const& err) {
        return os << "[Error] at " << err.expr.source().line << ':' << err.expr.source().col << ": " << err.msg;
    }
};

struct Closure {
    Closure(std::unordered_map<std::string, std::size_t> parameters,
            std::unordered_map<std::string, ast::Expr *> definitions, Closure *parent, ast::List *source)
        : m_parameters(std::move(parameters)),
          m_definitions(std::move(definitions)),
          m_parent(parent),
          m_source(source) {}

    Closure(Closure const&) = delete;
    Closure(Closure&&) = default;
    Closure& operator=(Closure const&) = delete;
    Closure& operator=(Closure&&) = default;

    void add_capture(std::string const& name) { m_captures.emplace(name, m_captures.size()); }

    [[nodiscard]] std::size_t header_size() const { return m_parameters.size() + m_captures.size(); }
    [[nodiscard]] std::size_t index_header(std::string const& name) const {
        if (auto it = m_parameters.find(name); it != m_parameters.end()) {
            return m_parameters.size() + m_captures.size() - 1 - it->second;
        }
        if (auto it = m_captures.find(name); it != m_captures.end()) {
            return m_captures.size() - 1 - it->second;
        }
        std::unreachable();
    }

    [[nodiscard]] std::vector<std::string> captures() const {
        std::vector<std::string> ordered(m_captures.size());
        for (auto& [name, i] : m_captures) {
            ordered[i] = name;
        }
        return ordered;
    }

    std::optional<std::string> name_for(Closure const& clsr) const {
        /*
            TODO:
            (define (main)
                (define (id x) x)
                (id 1))
            id here should be called main.id or something
        */

        for (auto& [name, source] : m_definitions) {
            if (source == &clsr.source()) {
                return name;
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] Closure const *parent() const { return m_parent; }

    [[nodiscard]] ast::List& source() const {
        if (not m_source) {
            throw std::logic_error("Internal compiler error.");
        }
        return *m_source;
    }

    [[nodiscard]] bool is_defined(std::string const& name) const {
        return m_parameters.contains(name) or m_definitions.contains(name);
    }
    [[nodiscard]] bool is_local(std::string const& name) const {
        return m_parameters.contains(name) or m_captures.contains(name);
    }

    [[nodiscard]] bool is_static(std::vector<std::unique_ptr<Closure>> const& closures, std::string const& name) const {
        if (m_parameters.contains(name) or m_captures.contains(name)) {
            return true;
        }
        if (not m_parent) {
            return false;
        }
        return m_parent->is_static_step(closures, name);
    }

private:
    [[nodiscard]] bool is_static_step(std::vector<std::unique_ptr<Closure>> const& closures,
                                      std::string const& name) const {
        if (auto it = m_definitions.find(name); it != m_definitions.end()) {
            for (auto& closure : closures) {
                if (closure->m_source == it->second) {
                    if (not closure->m_captures.empty()) {
                        return false;
                    }
                    break;
                }
            }
            return true;
        } else {
            if (not m_parent) {
                return false;
            }
            return m_parent->is_static_step(closures, name);
        }
    }

    std::unordered_map<std::string, std::size_t> m_parameters;
    std::unordered_map<std::string, std::size_t> m_captures;
    std::unordered_map<std::string, ast::Expr *> m_definitions;
    Closure *m_parent;
    ast::List *m_source;
};

struct Program {
    std::vector<std::unique_ptr<Closure>> closures;
};

/// @brief Checks program for unbound atoms and discovers closures, parameters and captures.
std::expected<Program, std::vector<StaticError>> do_program(std::vector<ast::ExprPtr> const& exprs);

#endif

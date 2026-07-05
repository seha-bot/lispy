#ifndef RAW_AST_HPP
#define RAW_AST_HPP

#include <cstdint>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace raw_ast {

struct Atom {
  std::string name;
};

struct Number {
  std::int64_t value;
};

struct Expr;

struct List {
  std::vector<Expr> elements;
};

using ExprBase = std::variant<Atom, List, Number>;

struct SourceLocation {
  int line;
  int col;

  friend std::ostream &operator<<(std::ostream &os, SourceLocation const &sl) {
    return os << sl.line << ':' << sl.col;
  }
};

struct SourceRange {
  SourceLocation first;
  SourceLocation last;

  friend std::ostream &operator<<(std::ostream &os, SourceRange const &sr) {
    return os << '[' << sr.first << ',' << sr.last << ']';
  }
};

struct Expr : ExprBase {
  Expr(Atom value, SourceRange source_range)
      : ExprBase(std::move(value)), m_source_range(source_range) {}

  Expr(List value, SourceRange source_range)
      : ExprBase(std::move(value)), m_source_range(source_range) {}

  Expr(Number value, SourceRange source_range)
      : ExprBase(std::move(value)), m_source_range(source_range) {}

  SourceRange source_range() const { return m_source_range; }

  friend std::ostream &operator<<(std::ostream &os, Expr const &expr) {
    expr.format(os, 1);
    return os;
  }

private:
  void format(std::ostream &os, int depth) const {
    struct Visitor {
      void operator()(Atom const &atom) { os << "Atom(\"" << atom.name << "\")"; }
      void operator()(List const &list) {
        os << "List(\n";
        for (auto &expr : list.elements) {
          for (int i = 0; i < depth; i++) {
            os << "  ";
          }
          expr.format(os, depth + 1);
          os << '\n';
        }
        for (int i = 1; i < depth; i++) {
          os << "  ";
        }
        os << ')';
      }
      void operator()(Number const &number) { os << "Number(" << number.value << ")"; }

      std::ostream &os;
      int depth;
    };
    os << m_source_range << ' ';
    std::visit(Visitor{os, depth}, *this);
  }

  SourceRange m_source_range;
};

} // namespace raw_ast

#endif

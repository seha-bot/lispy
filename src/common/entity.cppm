module;

#include <memory>
#include <string>
#include <variant>
#include <vector>

export module entity;

import id;
import tag;
import todo;

export namespace entity {

struct ValueDeclaration {
  std::string name;
  id::TypeId type_signature;
};

template <typename Expr> struct ValueDefinition {
  std::string name;
  std::unique_ptr<Expr> value;
};

template <typename Expr> struct MergedValueDefinition {
  std::string name;
  id::TypeId type_signature;
  std::unique_ptr<Expr> value;
};

struct TypeFormDefinition {
  std::string name;
  id::TypeId type;
};

struct Binding {
  std::string name;
  id::TypeId type_id;
};

struct TypeBinding {
  std::string name;
};

struct ModuleDefinition {
  std::string name;
};

template <typename Expr>
using ModuleEntityBase = std::variant<ValueDeclaration, ValueDefinition<Expr>,
                                      MergedValueDefinition<Expr>, ModuleDefinition>;

template <typename Expr> struct ModuleEntity : ModuleEntityBase<Expr> {
  using ModuleEntityBase<Expr>::ModuleEntityBase;

  std::string &name() {
    return std::visit([](auto &e) -> std::string & { return e.name; }, *this);
  }

  std::string const &name() const {
    return std::visit([](auto &e) -> std::string const & { return e.name; }, *this);
  }

  struct Context {
    std::vector<ModuleEntity<Expr>> const &entities;
    std::vector<TypeFormDefinition> const &forms;
    std::vector<tag::Tag> const &tags;
  };

  void format(std::ostream &os, Context ctx, std::size_t depth) const {
    struct Visitor {
      void operator()(ValueDeclaration const &val) {
        // TODO: Type info.
        os << "(dec " << val.name << ')';
      }
      void operator()(ValueDefinition<Expr> const &val) {
        os << "(def " << val.name << ' ';
        val.value->format(os, {ctx.entities, ctx.forms, ctx.tags}, depth);
        os << ')';
      }
      void operator()(MergedValueDefinition<Expr> const &val) {
        // TODO: Type info.
        os << "(dec " << val.name << ')';
        os << "(def " << val.name << ' ';
        val.value->format(os, {ctx.entities, ctx.forms, ctx.tags}, depth);
        os << ')';
      }
      void operator()(ModuleDefinition const &) { todo(); }

      std::ostream &os;
      Context ctx;
      std::size_t depth;
    };
    os << std::string(depth, ' ');
    std::visit(Visitor{os, ctx, depth}, *this);
  }
};

} // namespace entity

module;

#include <ostream>
#include <string>
#include <variant>
#include <vector>

export module formatter;

import entity;
import expr;
import id;
import tag;
import todo;
import type;
import type_storage;

export namespace formatter {

struct TypeContext {
  storage::TypeStorage const &ts;
  std::vector<entity::TypeFormDefinition> const &forms;
  std::vector<tag::Tag> const &tags;
};

struct Context {
  storage::TypeStorage const &ts;
  std::vector<entity::ModuleEntity<expr::Expr>> const &entities;
  std::vector<entity::TypeFormDefinition> const &forms;
  std::vector<tag::Tag> const &tags;
};

std::string type_name(TypeContext ctx, id::TypeId t) {
  struct Visitor {
    std::string operator()(type::Arrow const &b) {
      return "(" + type_name(ctx, b.from_id) + ") -> " + type_name(ctx, b.to_id);
    }
    std::string operator()(type::ForAll const &c) { return "\\." + type_name(ctx, c.type_id); }
    std::string operator()(type::DeBruijnIndex const &d) { return std::to_string(d.value); }
    std::string operator()(type::Variant const &v) {
      if (v.elements.empty()) {
        return "[]";
      }

      std::string str = "[";
      for (auto &[tag_id, type_id] : v.elements) {
        str += " " + ctx.tags[tag_id.value].name.substr(1) + ": " + type_name(ctx, type_id) + ";";
      }
      return str + " ]";
    }
    std::string operator()(type::Struct const &s) {
      if (s.elements.empty()) {
        return "{}";
      }
      std::string str = "{";
      for (auto &[tag_id, type_id] : s.elements) {
        str += " " + ctx.tags[tag_id.value].name.substr(1) + ": " + type_name(ctx, type_id) + ";";
      }
      return str + " }";
    }
    std::string operator()(type::Application const &) { todo(); }
    std::string operator()(type::Variable const &) {
      return "#" + std::to_string(ctx.ts.m_rep.representative(t).value);
    }
    std::string operator()(type::NamedTypeReference const &a) {
      return ctx.forms[a.definition_id.value].name;
    }

    TypeContext ctx;
    id::TypeId t;
  };
  return std::visit(Visitor{ctx, t}, ctx.ts.read(t));
}

void format_expr(std::ostream &os, Context ctx, std::size_t depth, expr::Expr const &expr) {
  struct Visitor {
    void operator()(expr::Application const &app) {
      os << '(';
      format_expr(os, ctx, 0, *app.function);
      os << ' ';
      format_expr(os, ctx, 0, *app.argument);
      os << ')';
    }
    void operator()(expr::Case const &) { todo(); }
    void operator()(expr::Variant const &v) {
      if (v.value) {
        os << '(' << ctx.tags[v.tag_id.value].name << ' ';
        format_expr(os, ctx, 0, **v.value);
        os << ')';
      } else {
        os << ctx.tags[v.tag_id.value].name;
      }
    }
    void operator()(expr::Pack const &) { todo(); }
    void operator()(expr::Lambda const &l) {
      os << "(lambda (" << l.binding->name << ' '
         << type_name({ctx.ts, ctx.forms, ctx.tags}, l.binding->type_id) << ") ";
      format_expr(os, ctx, 0, *l.body);
      os << ')';
    }
    void operator()(expr::TVLambda const &l) {
      os << "(tv-lambda ";
      format_expr(os, ctx, 0, *l.body);
      os << ')';
    }
    void operator()(expr::ValueReference const &v) {
      os << ctx.entities[v.value_entity_id.value].name();
    }
    void operator()(expr::BindingReference const &b) { os << b.binding.get().name; }

    std::ostream &os;
    Context ctx;
    std::size_t depth;
  };
  os << std::string(depth, ' ');
  std::visit(Visitor{os, ctx, depth}, expr);
}

void format_entity(std::ostream &os, Context ctx, std::size_t depth,
                   entity::ModuleEntity<expr::Expr> const &entity) {
  struct Visitor {
    void operator()(entity::ValueDeclaration const &val) {
      os << "(dec " << val.name << ' '
         << type_name({ctx.ts, ctx.forms, ctx.tags}, val.type_signature) << ')';
    }
    void operator()(entity::ValueDefinition<expr::Expr> const &val) {
      os << "(def " << val.name << ' ';
      format_expr(os, ctx, depth, *val.value);
      os << ')';
    }
    void operator()(entity::MergedValueDefinition<expr::Expr> const &val) {
      os << "(dec " << val.name << ' '
         << type_name({ctx.ts, ctx.forms, ctx.tags}, val.type_signature) << ")\n";
      os << "(def " << val.name << ' ';
      format_expr(os, ctx, depth, *val.value);
      os << ')';
    }
    void operator()(entity::ModuleDefinition const &) { todo(); }

    std::ostream &os;
    Context ctx;
    std::size_t depth;
  };
  os << std::string(depth, ' ');
  std::visit(Visitor{os, ctx, depth}, entity);
}

} // namespace formatter

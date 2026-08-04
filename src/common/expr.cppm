module;

#include <functional>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

export module expr;

import entity;
import id;
import tag;
import todo;

export namespace expr {

struct Expr;

struct Application {
  std::unique_ptr<Expr> function;
  std::unique_ptr<Expr> argument;
};

struct Case {
  struct Pattern {
    id::TagId tag;
    std::vector<entity::Binding> bindings;
  };

  struct Choice;

  std::unique_ptr<Expr> scrutinee;
  std::vector<Choice> choices;
};

struct Variant {
  id::TagId tag_id;
  std::optional<std::unique_ptr<Expr>> value;
};

// This looks a lot like Variant above. Hmmm...
struct TaggedValue {
  id::TagId tag_id;
  std::unique_ptr<Expr> value;
};

struct Pack {
  std::vector<TaggedValue> tagged_values;
};

struct Lambda {
  std::vector<std::reference_wrapper<entity::Binding const>> captures;
  std::unique_ptr<entity::Binding> binding;
  std::unique_ptr<Expr> body;
};

struct TVLambda {
  std::unique_ptr<Expr> body;
};

struct ValueReference {
  // std::variant<entity::ValueDeclaration const *, entity::ValueDefinition const *,
  //              entity::MergedValueDefinition const *>
  //     entity;
  // TODO: This is guaranteed to be one from the comment above.
  id::EntityId value_entity_id;
};

struct BindingReference {
  std::reference_wrapper<entity::Binding const> binding;
};

using ExprBase = std::variant<Application, Case, Variant, Pack, Lambda, TVLambda, ValueReference,
                              BindingReference>;
struct Expr : ExprBase {
  using ExprBase::ExprBase;

  struct Context {
    std::vector<entity::ModuleEntity<Expr>> const &entities;
    std::vector<entity::TypeFormDefinition> const &forms;
    std::vector<tag::Tag> const &tags;
  };

  void format(std::ostream &os, Context ctx, std::size_t depth) const {
    struct Visitor {
      void operator()(Application const &app) {
        os << '(';
        app.function->format(os, ctx, 0);
        os << ' ';
        app.argument->format(os, ctx, 0);
        os << ')';
      }
      void operator()(Case const &) { todo(); }
      void operator()(Variant const &v) {
        if (v.value) {
          os << '(' << ctx.tags[v.tag_id.value].name << ' ';
          (*v.value)->format(os, ctx, 0);
          os << ')';
        } else {
          os << ctx.tags[v.tag_id.value].name;
        }
      }
      void operator()(Pack const &) { todo(); }
      void operator()(Lambda const &l) {
        // TODO: Type info.
        os << "(lambda " << l.binding->name << ' ';
        l.body->format(os, ctx, 0);
        os << ')';
      }
      void operator()(TVLambda const &) { todo(); }
      void operator()(ValueReference const &v) {
        os << ctx.entities[v.value_entity_id.value].name();
      }
      void operator()(BindingReference const &b) { os << b.binding.get().name; }

      std::ostream &os;
      Context ctx;
      std::size_t depth;
    };
    os << std::string(depth, ' ');
    std::visit(Visitor{os, ctx, depth}, *this);
  }
};

struct Case::Choice {
  Pattern pattern;
  Expr arm;
};

} // namespace expr

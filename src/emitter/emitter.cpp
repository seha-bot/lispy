#include "emitter.hpp"

#include <cassert>
#include <concepts>
#include <string_view>
#include <variant>

#include "ast.hpp"
#include "storage/resolved.hpp"
#include "vm_types.hpp"

namespace emitter {

template <vm::info::OperandType T, typename... Ts>
concept OperandTypeMatchesImpl = //
    (T == vm::info::OperandType::none and sizeof...(Ts) == 0) or
    (T == vm::info::OperandType::number and sizeof...(Ts) == 1 and
     (std::same_as<Ts, vm::Byte::underlying> and ...)) or
    (T == vm::info::OperandType::label and sizeof...(Ts) == 1 and
     (std::convertible_to<Ts, std::string_view> and ...)) or
    (T == vm::info::OperandType::table and sizeof...(Ts) == 1 and
     (std::convertible_to<Ts, std::string_view> and ...));

template <vm::Mnemonic M, typename... Ts>
concept OperandTypeMatches = OperandTypeMatchesImpl<vm::mnemonic_operand_type(M), Ts...>;

struct NoOperand {};

struct NumberOperand {
  vm::Byte operand;
};

struct LabelOperand {
  std::string operand;
};

struct Instruction {
  void format(std::ostream &os) const {
    os << vm::mnemonic_to_string(mnemonic);
    struct Visitor {
      void operator()(NoOperand const &) {}
      void operator()(NumberOperand const &op) { os << ' ' << op.operand.value(); }
      void operator()(LabelOperand const &op) { os << ' ' << op.operand; }

      std::ostream &os;
    };
    std::visit(Visitor{os}, operand);
  }

  vm::Mnemonic mnemonic;
  std::variant<NoOperand, NumberOperand, LabelOperand> operand;
};

struct Label {
  void format(std::ostream &os) const { os << ".L" << id; }
  std::string name() const { return ".L" + std::to_string(id); }
  std::size_t id;
};

struct Subroutine {
  Subroutine(std::string name, std::vector<ast::EntityId> stack_header) : m_name(std::move(name)) {
    manual_stack_push(std::move(stack_header));
  }

  Subroutine(Subroutine const &) = delete;
  Subroutine &operator=(Subroutine const &) = delete;

  Subroutine(Subroutine &&) = delete;
  Subroutine &operator=(Subroutine &&) = delete;

  template <vm::Mnemonic M, typename... Ts>
  void push(Ts... operands)
    requires OperandTypeMatches<M, Ts...>
  {
    ((void)operands, ...);
    if constexpr (sizeof...(Ts) == 0) {
      m_lines.push_back(Instruction{M, NoOperand{}});
    } else if constexpr ((std::same_as<Ts, vm::Byte::underlying> and ...)) {
      m_lines.push_back(Instruction{M, NumberOperand{vm::Byte(operands...)}});
    } else {
      m_lines.push_back(Instruction{M, LabelOperand{std::string(operands...)}});
    }

    auto const sd = vm::mnemonic_stack_delta(M);
    if (sd < 0) {
      auto const abs_sd = std::size_t(-sd);
      if (m_stack_size < abs_sd) {
        todo();
      }
      m_stack_size -= abs_sd;
    } else {
      auto const abs_sd = std::size_t(sd);
      m_stack_size += abs_sd;
    }
  }

  std::string const &name() const { return m_name; }

  Label make_local_label() { return Label{m_label_count++}; }

  void push_label(Label label) { m_lines.push_back(std::move(label)); }

  void manual_stack_push(std::vector<ast::EntityId> entities) {
    for (auto &entity_id : entities) {
      m_binding_stack_indexes.insert({entity_id, m_stack_size++});
    }
  }

  // TODO: It'd be splendid if this could take a binding instead of an entity id.
  std::size_t get_relative_stack_index(ast::EntityId entity_id) const {
    auto const it = m_binding_stack_indexes.find(entity_id);
    if (it == m_binding_stack_indexes.end()) {
      todo();
    }
    auto const stack_position = it->second;
    if (stack_position >= m_stack_size) {
      todo();
    }
    return m_stack_size - 1 - stack_position;
  }

  void format(std::ostream &os) const {
    os << m_name << ":\n";
    for (auto &line : m_lines) {
      struct Visitor {
        void operator()(Instruction const &instruction) {
          os << "  ";
          instruction.format(os);
        }
        void operator()(Label const &label) { label.format(os); }
        std::ostream &os;
      };
      std::visit(Visitor{os}, line);
      os << '\n';
    }
  }

private:
  std::string m_name;
  std::vector<std::variant<Instruction, Label>> m_lines;
  std::unordered_map<ast::EntityId, std::size_t> m_binding_stack_indexes;
  std::size_t m_stack_size = 0;
  std::size_t m_label_count = 0;
};

struct Table {
  Table(std::string name) : m_name(std::move(name)) {}

  Table(Table const &) = delete;
  Table &operator=(Table const &) = delete;

  Table(Table &&) = delete;
  Table &operator=(Table &&) = delete;

  void push(Label label) { m_labels.push_back(std::move(label)); }
  std::string const &name() const { return m_name; }

  void format(std::ostream &os) const {
    os << m_name << " table";
    for (auto &label : m_labels) {
      os << ' ' << label.name();
    }
  }

private:
  std::vector<Label> m_labels;
  // TODO: u prolly want this to be a std::size_t.
  std::string m_name;
};

struct Code {
  Subroutine &start_lambda_definition(ast::Lambda const &lambda) {
    lambda_subroutines[&lambda] = subroutines.size();

    auto stack_header = lambda.captures;
    stack_header.push_back(lambda.binding);
    subroutines.push_back(std::make_unique<Subroutine>("lambda" + std::to_string(lambda_cnt++),
                                                       std::move(stack_header)));
    return *subroutines.back();
  }

  Table &make_table() {
    tables.push_back(std::make_unique<Table>("table" + std::to_string(tables.size())));
    return *tables.back();
  }

  std::string const *get_definition(ast::Lambda const &lambda) const {
    auto it = lambda_subroutines.find(&lambda);
    if (it == lambda_subroutines.end()) {
      return nullptr;
    }
    return &subroutines[it->second]->name();
  }

  void format(std::ostream &os) const {
    os << "section .data\n";
    for (auto &table : tables) {
      os << "  ";
      table->format(os);
      os << '\n';
    }

    os << "section .code\n";
    for (auto &subroutine : subroutines) {
      subroutine->format(os);
    }
  }

  std::vector<std::unique_ptr<Subroutine>> subroutines;
  std::unordered_map<ast::Lambda const *, std::size_t> lambda_subroutines;
  std::vector<std::unique_ptr<Table>> tables;
  std::size_t lambda_cnt = 0;
  std::size_t label_cnt = 0;
};

struct Context {
  ast::Entity const &entity(ast::EntityId entity_id) const {
    return ast.entities.at(entity_id.value);
  }
  std::vector<ast::EntityId> const &captures(ast::Lambda const &lambda) const {
    return lambda.captures;
  }

  ast::type::Variant const &get_variant(ast::TypeId type_id) const {
    // SAFETY: This is assumed to be a variant at this point.
    auto &r = ast.ts->read(type_id);
    if (auto *variant = std::get_if<ast::type::Variant>(&r.unnamed_part())) {
      return *variant;
    } else if (auto *named_reference = std::get_if<ast::type::NamedTypeReference>(&r.unnamed_part())) {
      auto &def = std::get<ast::TypeFormDefinition>(entity(named_reference->definition_id));
      return get_variant(def.type);
    }
    std::unreachable();
  }
  std::size_t discriminator_index(ast::TypeId type_id, ast::TagId tag_id) const {
    auto &type = get_variant(type_id);
    std::size_t index = 0;
    for (auto &[element_tag_id, _] : type.elements) {
      if (element_tag_id == tag_id) {
        return index;
      }
      ++index;
    }
    // SAFETY: It is assumed that the given tag_id exists in the variant.
    std::unreachable();
  }

  storage::ResolvedAST const &ast;
  storage::TypeEnv const &type_env;
  Code code{};
};

namespace {

void compile_entity(Context &ctx, Subroutine &sub, ast::EntityId entity_id);

void compile_expr(Context &ctx, Subroutine &sub, ast::Expr const &expr) {
  struct Visitor {
    void operator()(ast::Application const &call) {
      compile_expr(ctx, sub, *call.callee);
      assert(not call.arguments.empty());
      for (auto &arg : call.arguments) {
        compile_expr(ctx, sub, arg);
        sub.push<vm::Mnemonic::callind>();
      }
    }
    void operator()(ast::Case const &case_) {
      compile_expr(ctx, sub, *case_.scrutinee);
      auto &table = ctx.code.make_table();
      sub.push<vm::Mnemonic::jmpvd>(table.name());
      auto end_label = sub.make_local_label();

      // FIX: this has ordering issues:
      // (form Bool :true :false)
      // (case :true :false x :true y)
      // might evaluate to x.
      for (auto &choice : case_.choices) {
        std::vector<ast::EntityId> stack_header;
        if (choice.pattern.bindings.size() == 1) {
          stack_header.push_back(choice.pattern.bindings[0]);
        } else if (choice.pattern.bindings.size() > 1) {
          todo();
        }

        auto choice_label = sub.make_local_label();
        sub.push_label(choice_label);
        table.push(choice_label);
        sub.manual_stack_push(std::move(stack_header));

        compile_expr(ctx, sub, choice.arm);
        sub.push<vm::Mnemonic::seti>(vm::Byte::underlying(1));
        sub.push<vm::Mnemonic::jmp>(end_label.name());
      }
      sub.push_label(std::move(end_label));
    }
    void operator()(ast::Variant const &variant) {
      todo();
      // if (label_call.argument) {
      //   compile_expr(ctx, sub, **label_call.argument);
      // } else {
      //   sub.push<vm::Mnemonic::push>(vm::Byte::underlying(0));
      // }
      //
      // std::size_t discriminator_id = ctx.discriminator_index(label_call.type, label_call.tag);
      // sub.push<vm::Mnemonic::push>(discriminator_id);
      //
      // sub.push<vm::Mnemonic::mkv>();
    }
    void operator()(ast::Pack const &pack) { todo(); }
    void operator()(ast::Lambda const &lambda) {
      auto name = [&] {
        // This happens when you recursively visit a lambda
        if (auto *def = ctx.code.get_definition(lambda)) {
          return *def;
        }

        auto &lambda_sub = ctx.code.start_lambda_definition(lambda);
        compile_expr(ctx, lambda_sub, *lambda.body);
        lambda_sub.push<vm::Mnemonic::seti>(vm::Byte::underlying(lambda.captures.size() + 1));
        for (std::size_t i = 0; i < lambda.captures.size(); ++i) {
          lambda_sub.push<vm::Mnemonic::pop>();
        }
        lambda_sub.push<vm::Mnemonic::ret>();
        return lambda_sub.name();
      }();

      sub.push<vm::Mnemonic::pushs>(std::move(name));
      auto &captures = ctx.captures(lambda);
      for (auto &capture : captures) {
        // compile_entity(ctx, sub, capture);
        sub.push<vm::Mnemonic::pushi>(sub.get_relative_stack_index(capture));
      }
      sub.push<vm::Mnemonic::push>(1 + captures.size());
      sub.push<vm::Mnemonic::mka>();
    }
    void operator()(ast::TVLambda const &) { todo(); }
    void operator()(ast::EntityReference const &entity_ref) {
      return compile_entity(ctx, sub, entity_ref.id);
    }

    Context &ctx;
    Subroutine &sub;
  };
  return std::visit(Visitor{ctx, sub}, expr);
}

void compile_entity(Context &ctx, Subroutine &sub, ast::EntityId entity_id) {
  struct Visitor {
    void operator()(ast::ValueDefinition const &def) { return compile_expr(ctx, sub, def.value); }
    void operator()(ast::TypeFormDefinition const &) { todo(); }
    void operator()(ast::ValueDeclaration const &) { todo(); }
    void operator()(ast::MergedValueDefinition const &def) {
      return compile_expr(ctx, sub, def.value);
    }
    void operator()(ast::ModuleDefinition const &) { todo(); }
    void operator()(ast::Binding const &) {
      sub.push<vm::Mnemonic::pushi>(sub.get_relative_stack_index(entity_id));
    }
    void operator()(ast::TypeBinding const &) { todo(); }

    Context &ctx;
    Subroutine &sub;
    ast::EntityId entity_id;
  };
  return std::visit(Visitor{ctx, sub, entity_id}, ctx.entity(entity_id));
}

} // namespace

void emit(storage::ResolvedAST const &ast, storage::TypeEnv const &type_env, std::ostream &os) {
  Context ctx{ast, type_env};
  Subroutine sub("fuck", {});
  for (auto &entity_id : ast.global_module.entities) {
    if (std::holds_alternative<ast::ValueDefinition>(ctx.entity(entity_id)) or
        std::holds_alternative<ast::MergedValueDefinition>(ctx.entity(entity_id))) {
      compile_entity(ctx, sub, entity_id);
    }
  }

  ctx.code.format(os);

  // TODO: assert that sub.instructions.empty()
}

} // namespace emitter

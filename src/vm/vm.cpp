#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <fstream>
#include <iostream>
#include <optional>
#include <utility>
#include <vector>

#include "bytecoder.hpp"
#include "gc.hpp"
#include "todo.hpp"
#include "vm_types.hpp"

namespace {

struct Machine {
  vm::Byte &unsafe_seek(std::size_t i) { return m_stack.at(m_stack.size() - 1 - i); }
  vm::Byte &unsafe_top() { return unsafe_seek(0); }

  void push(vm::Byte obj) { m_stack.push_back(obj); }
  void push_call(vm::InstrPtr ip) { m_return_stack.push_back(ip); }

  vm::Byte unsafe_pop() {
    auto top = m_stack.back();
    m_stack.pop_back();
    return top;
  }
  vm::InstrPtr unsafe_pop_call() {
    auto ip = m_return_stack.back();
    m_return_stack.pop_back();
    return ip;
  }

  // void unsafe_pop_n(std::size_t n) { m_stack.resize(m_stack.size() - n); }

  bool has(std::size_t n) const { return n <= m_stack.size(); }
  std::size_t call_depth() const { return m_return_stack.size(); }

private:
  std::vector<vm::Byte> m_stack;
  std::vector<vm::InstrPtr> m_return_stack;
};

enum class StepResult {
  stack_overrun,
  call_stack_overrun,
  mismatched_type,
  corrupted_opcode,
  invalid_jump_address,
};

std::ostream &operator<<(std::ostream &os, StepResult sr) {
  switch (sr) {
  case StepResult::stack_overrun:
    return os << "stack_overrun";
  case StepResult::call_stack_overrun:
    return os << "call_stack_overrun";
  case StepResult::mismatched_type:
    return os << "mismatched_type";
  case StepResult::corrupted_opcode:
    return os << "corrupted_opcode";
  case StepResult::invalid_jump_address:
    return os << "invalid_jump_address";
  }
  std::unreachable();
}

struct Decoder {
  vm::Mnemonic opcode(vm::InstrPtr ip) const { return bytecoder::opcode(bc, ip); }
  vm::Byte operand(vm::InstrPtr ip) const { return bytecoder::operand(bc, ip); }

  std::size_t instr_size(vm::Mnemonic mnemonic, std::optional<vm::Byte> operand = std::nullopt) {
    return bytecoder::instr_size(vm::Instruction{mnemonic, operand.value_or(vm::Byte(0))});
  }

  bytecoder::Bytecode bc;
};

std::expected<vm::InstrPtr, StepResult> step(GC &gc, Machine &m, Decoder &dec,
                                             vm::InstrPtr ip) noexcept {
  auto opcode = dec.opcode(ip);
  switch (opcode) {
  case vm::Mnemonic::push: {
    auto value = dec.operand(ip);
    m.push(value);
    return ip.advance(dec.instr_size(vm::Mnemonic::push, value));
  }
  case vm::Mnemonic::pushi: {
    auto i = dec.operand(ip);
    if (not m.has(i.value() + 1)) {
      return std::unexpected(StepResult::stack_overrun);
    }
    m.push(m.unsafe_seek(i.value()));
    return ip.advance(dec.instr_size(vm::Mnemonic::pushi, i));
  }
  case vm::Mnemonic::pushs:
    todo();
  case vm::Mnemonic::seti: {
    auto i = dec.operand(ip);
    if (not m.has(i.value() + 1)) {
      return std::unexpected(StepResult::stack_overrun);
    }
    m.unsafe_seek(i.value()) = m.unsafe_top();
    return ip.advance(dec.instr_size(vm::Mnemonic::seti, i));
  }
  case vm::Mnemonic::pop: {
    if (not m.has(1)) {
      return std::unexpected(StepResult::stack_overrun);
    }
    m.unsafe_pop();
    return ip.advance(dec.instr_size(vm::Mnemonic::pop));
  }
  case vm::Mnemonic::call: {
    auto dest_ip = dec.operand(ip);
    m.push_call(ip.advance(dec.instr_size(vm::Mnemonic::call, dest_ip)));
    return vm::InstrPtr(dest_ip);
  }
  case vm::Mnemonic::ret: {
    if (m.call_depth() == 0) {
      return std::unexpected(StepResult::call_stack_overrun);
    }
    return m.unsafe_pop_call();
  }
  case vm::Mnemonic::mkv:
    todo();
  case vm::Mnemonic::setvd:
    todo();
  case vm::Mnemonic::setvv:
    todo();
  case vm::Mnemonic::jmpvd:
    todo();
  }

  return std::unexpected(StepResult::corrupted_opcode);
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: lispy <executable>\n";
    return EXIT_FAILURE;
  }

  Decoder dec;
  {
    std::ifstream in(argv[1], std::ios::binary);
    if (!in) {
      std::cerr << "Can't open file for reading.\n";
      return EXIT_FAILURE;
    }

    while (true) {
      auto b = in.get();
      if (not in) {
        break;
      }
      dec.bc.push_back(static_cast<unsigned char>(b));
    }
  }

  GC gc;
  Machine m;
  m.push_call(vm::InstrPtr(vm::Byte(dec.bc.size())));

  vm::InstrPtr ip(vm::Byte(bytecoder::read_n(dec.bc, 0, 8)));
  while (true) {
    if (ip.value().value() >= dec.bc.size()) {
      break;
    }

    auto next_ip = step(gc, m, dec, ip);
    if (not next_ip) {
      std::cout << "[Error] at \"" << to_string(dec.opcode(ip)) << "\": " << next_ip.error()
                << '\n';
      return EXIT_FAILURE;
    }
    ip = *next_ip;
  }
}

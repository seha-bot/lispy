#ifndef BYTECODER_HPP
#define BYTECODER_HPP

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <vector>

#include "vm_types.hpp"

namespace bytecoder {

inline std::size_t instr_size(vm::Instruction instr) {
  if (vm::mnemonic_operand_type(instr.mnemonic) == vm::OperandType::none) {
    return 1;
  } else {
    return 9;
  }
}

inline void write_n(std::ostream &os, std::uint64_t val, std::size_t n) {
  for (std::size_t i = 0; i < n; ++i) {
    os.put(static_cast<char>((val >> ((n - 1 - i) * 8)) & 0xFF));
  }
}

using Bytecode = std::vector<std::uint8_t>;

inline std::uint64_t read_n(Bytecode const &bc, std::size_t at, std::size_t n) {
  std::uint64_t res = 0;
  for (std::size_t i = 0; i < n; ++i) {
    res <<= 8;
    res |= bc[at + i];
  }
  return res;
}

inline vm::Mnemonic opcode(Bytecode const &bc, vm::InstrPtr ip) {
  return vm::Mnemonic(read_n(bc, ip.value().value(), 1));
}

inline vm::Byte operand(Bytecode const &bc, vm::InstrPtr ip) {
  return vm::Byte(read_n(bc, ip.value().value() + 1, 8));
}

} // namespace bytecoder

#endif

#ifndef VM_TYPES_HPP
#define VM_TYPES_HPP

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace vm {

struct Byte {
  explicit Byte(std::size_t value) : m_value(value) {}

  std::size_t value() const { return m_value; }

private:
  std::size_t m_value;
};

/// @brief Represents an index of an instruction inside bytecode.
struct InstrPtr {
  explicit InstrPtr(Byte value) : m_value(value) {}

  InstrPtr advance(std::size_t n) const { return InstrPtr(Byte(m_value.value() + n)); }
  Byte value() const { return m_value; }

private:
  Byte m_value;
};

enum class Mnemonic {
  // Stack manipulation
  push,
  pushi,
  pushs,
  seti,
  pop,

  // Subroutines
  call,
  ret,

  // Variants
  mkv,
  jmpvd,
};

inline std::string to_string(Mnemonic m) {
  switch (m) {
  case Mnemonic::push:
    return "push";
  case Mnemonic::pushi:
    return "pushi";
  case Mnemonic::pushs:
    return "pushs";
  case Mnemonic::seti:
    return "seti";
  case Mnemonic::pop:
    return "pop";
  case Mnemonic::call:
    return "call";
  case Mnemonic::ret:
    return "ret";
  case Mnemonic::mkv:
    return "mkv";
  case Mnemonic::jmpvd:
    return "jmpvd";
  }
  return "unknown";
}

inline std::optional<Mnemonic> mnemonic_from_string(std::string_view s) {
  if (s == "push") {
    return Mnemonic::push;
  } else if (s == "pushi") {
    return Mnemonic::pushi;
  } else if (s == "pushs") {
    return Mnemonic::pushs;
  } else if (s == "seti") {
    return Mnemonic::seti;
  } else if (s == "pop") {
    return Mnemonic::pop;
  } else if (s == "call") {
    return Mnemonic::call;
  } else if (s == "ret") {
    return Mnemonic::ret;
  } else if (s == "mkv") {
    return Mnemonic::mkv;
  } else if (s == "jmpvd") {
    return Mnemonic::jmpvd;
  } else {
    return std::nullopt;
  }
}

enum class OperandType {
  none,
  number,
  label,
};

inline OperandType mnemonic_operand_type(Mnemonic m) {
  switch (m) {
  case Mnemonic::push:
    return OperandType::number;
  case Mnemonic::pushi:
    return OperandType::number;
  case Mnemonic::pushs:
    return OperandType::label;
  case Mnemonic::seti:
    return OperandType::number;
  case Mnemonic::pop:
    return OperandType::none;
  case Mnemonic::call:
    return OperandType::label;
  case Mnemonic::ret:
    return OperandType::none;
  case Mnemonic::mkv:
    return OperandType::none;
  case Mnemonic::jmpvd:
    return OperandType::none;
  }

  std::unreachable();
}

struct Instruction {
  Mnemonic mnemonic;
  Byte operand;
};

} // namespace vm

#endif

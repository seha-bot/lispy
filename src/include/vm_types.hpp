#ifndef VM_TYPES_HPP
#define VM_TYPES_HPP

#include <cstddef>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace vm {

struct Byte {
  using underlying = std::size_t;

  explicit Byte(underlying value) : m_value(value) {}

  underlying value() const { return m_value; }

private:
  underlying m_value;
};

/// @brief Represents an index of an instruction inside bytecode.
struct InstrPtr {
  explicit InstrPtr(Byte value) : m_value(value) {}

  InstrPtr advance(std::size_t n) const { return InstrPtr(Byte(m_value.value() + n)); }
  Byte value() const { return m_value; }

private:
  Byte m_value;
};

enum class Mnemonic : unsigned char {
  // Stack manipulation
  push,
  pushi,
  pushs,
  seti,
  pop,

  // Subroutines
  call,
  callind,
  ret,
  jmp,

  // Arrays
  mka,
  seta,
  geta,

  // Variants
  mkv,
  jmpvd,
};

namespace info {

enum class OperandType : unsigned char {
  none,
  number,
  label,
  table,
};

struct MnemonicInfo {
  Mnemonic mnemonic;
  OperandType operand_type;
  char const *display_name;
  unsigned char pop_cnt, push_cnt;
};

using enum Mnemonic;
using enum OperandType;

// Syntax: operand? | popped_values... -> pushed_values...
constexpr MnemonicInfo mnemonic_info[] = {
    // value | -> value
    {push, number, "push", 0, 1},
    // index | -> stack[index]
    {pushi, number, "pushi", 0, 1},
    // label | -> label_reference
    {pushs, label, "pushs", 0, 1},
    // index | value ->
    {seti, number, "seti", 1, 0},
    // | value ->
    {pop, none, "pop", 1, 0},
    // label | ->
    {call, label, "call", 0, 0},
    // | callable ->
    {callind, none, "callind", 1, 0},
    // | ->
    {ret, none, "ret", 0, 0},
    // label | ->
    {jmp, label, "jmp", 0, 0},
    // | size -> array_ref
    {mka, none, "mka", 1, 1},
    // | array_ref index value ->
    {seta, none, "seta", 3, 0},
    // | array_ref index -> value
    {geta, none, "geta", 2, 1},
    // | value discriminator -> variant_ref
    {mkv, none, "mkv", 2, 1},
    // table | variant_ref -> variant_value
    {jmpvd, table, "jmpvd", 1, 1},
};

} // namespace info

constexpr std::string mnemonic_to_string(Mnemonic m) {
  return info::mnemonic_info[std::to_underlying(m)].display_name;
}

constexpr std::optional<Mnemonic> mnemonic_from_string(std::string_view s) {
  auto it = std::ranges::find(info::mnemonic_info, s, [](auto &info) { return info.display_name; });
  if (it == std::ranges::end(info::mnemonic_info)) {
    return std::nullopt;
  }
  return it->mnemonic;
}

constexpr info::OperandType mnemonic_operand_type(Mnemonic m) {
  return info::mnemonic_info[std::to_underlying(m)].operand_type;
}

constexpr int mnemonic_stack_delta(Mnemonic m) {
  auto &mi = info::mnemonic_info[std::to_underlying(m)];
  return mi.push_cnt - mi.pop_cnt;
}

struct Instruction {
  Mnemonic mnemonic;
  Byte operand;
};

} // namespace vm

#endif

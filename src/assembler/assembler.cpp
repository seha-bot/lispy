#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "bytecoder.hpp"
#include "vm_types.hpp"

namespace {

[[noreturn]] void fail(int line_num, std::string_view msg) {
  std::cerr << "Error at line " << line_num << ": " << msg << '\n';
  std::exit(EXIT_FAILURE);
};

struct Empty {};

struct Label {
  std::string name;
};

struct NoOperandInstruction {
  vm::Mnemonic mnemonic;
};

struct NumberOperandInstruction {
  vm::Mnemonic mnemonic;
  vm::Byte operand;
};

struct LabelOperandInstruction {
  vm::Mnemonic mnemonic;
  std::string operand;
};

using InstructionBase =
    std::variant<NoOperandInstruction, NumberOperandInstruction, LabelOperandInstruction>;
struct Instruction : InstructionBase {

  using InstructionBase::variant;

  vm::Mnemonic mnemonic() const {
    return std::visit([](auto &i) { return i.mnemonic; }, *this);
  }
};

using ParseResult = std::variant<Empty, Label, NoOperandInstruction, NumberOperandInstruction,
                                 LabelOperandInstruction>;

std::vector<std::string> parse_words(std::string line) {
  std::vector<std::string> words;
  std::size_t i = 0;
  while (true) {
    while (i < line.size() and std::isspace(static_cast<unsigned char>(line[i]))) {
      ++i;
    }
    if (i == line.size() or line[i] == ';') {
      break;
    }

    std::string word;
    while (i < line.size() and not std::isspace(static_cast<unsigned char>(line[i])) and
           line[i] != ';') {
      word.push_back(line[i]);
      ++i;
    }
    words.push_back(std::move(word));
  }
  return words;
}

ParseResult parse_line(int line_num, std::string line) {
  auto words = parse_words(std::move(line));
  if (words.empty()) {
    return Empty{};
  }

  if (words[0].back() == ':') {
    if (words.size() != 1) {
      fail(line_num, "Trailing tokens after label.");
    }
    words[0].pop_back();
    return Label{std::move(words[0])};
  }

  auto mnemonic = [&] {
    auto result = vm::mnemonic_from_string(words[0]);
    if (not result) {
      fail(line_num, "Unknown mnemonic.");
    }
    return *result;
  }();

  switch (vm::mnemonic_operand_type(mnemonic)) {
  case vm::OperandType::none:
    return NoOperandInstruction{mnemonic};
  case vm::OperandType::number: {
    if (words.size() != 2) {
      fail(line_num, "Expected operand.");
    }
    std::size_t number;
    // TODO: check result
    std::from_chars(words[1].data(), words[1].data() + words[1].size(), number);
    return NumberOperandInstruction{mnemonic, vm::Byte(number)};
  }
  case vm::OperandType::label: {
    if (words.size() != 2) {
      fail(line_num, "Expected operand.");
    }
    return LabelOperandInstruction{mnemonic, std::move(words[1])};
  }
  }

  std::unreachable();
}

void assemble(std::ostream &os, vm::InstrPtr entry_point, std::vector<Instruction> const &instrs,
              std::unordered_map<std::string, vm::InstrPtr> const &global_labels) {
  bytecoder::write_n(os, entry_point.value().value(), 8);

  for (auto &instruction : instrs) {
    bytecoder::write_n(os, static_cast<std::uint64_t>(instruction.mnemonic()), 1);
    struct Visitor {
      void operator()(NoOperandInstruction) {}
      void operator()(NumberOperandInstruction instr) {
        bytecoder::write_n(os, static_cast<std::uint64_t>(instr.operand.value()), 8);
      }
      void operator()(LabelOperandInstruction instr) {
        auto it = global_labels.find(instr.operand);
        if (it == global_labels.end()) {
          // TODO: what to do here man...
          fail(-1, "Label \"" + instr.operand + "\" not found.");
        }
        bytecoder::write_n(os, static_cast<std::uint64_t>(it->second.value().value()), 8);
      }

      std::ostream &os;
      std::unordered_map<std::string, vm::InstrPtr> const &global_labels;
    };
    std::visit(Visitor{os, global_labels}, instruction);
  }
}

} // namespace

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: assembler <dest> <source>\n";
    return EXIT_FAILURE;
  }

  std::ofstream out(argv[1], std::ios::binary);
  if (!out) {
    std::cerr << "Can't open file for writing.\n";
    return EXIT_FAILURE;
  }

  std::ifstream in(argv[2]);
  if (!in) {
    std::cerr << "Can't open file for reading.\n";
    return EXIT_FAILURE;
  }

  std::unordered_map<std::string, vm::InstrPtr> global_labels;
  std::vector<Instruction> instructions;
  vm::InstrPtr ip(vm::Byte(8));
  int line_num = 0;

  while (true) {
    ++line_num;
    std::string line;
    if (not std::getline(in, line)) {
      break;
    }
    auto result = parse_line(line_num, std::move(line));

    struct Visitor {
      void operator()(Empty) {}
      void operator()(Label label) { global_labels.insert({std::move(label.name), ip}); }
      void operator()(NoOperandInstruction instr) {
        instructions.push_back(instr);
        ip = ip.advance(bytecoder::instr_size(vm::Instruction{instr.mnemonic, vm::Byte(0)}));
      }
      void operator()(NumberOperandInstruction instr) {
        instructions.push_back(instr);
        ip = ip.advance(bytecoder::instr_size(vm::Instruction{instr.mnemonic, instr.operand}));
      }
      void operator()(LabelOperandInstruction instr) {
        instructions.push_back(instr);
        ip = ip.advance(bytecoder::instr_size(vm::Instruction{instr.mnemonic, vm::Byte(0)}));
      }

      std::unordered_map<std::string, vm::InstrPtr> &global_labels;
      std::vector<Instruction> &instructions;
      vm::InstrPtr &ip;
      int &line_num;
    };
    std::visit(Visitor{global_labels, instructions, ip, line_num}, result);
  }

  if (not global_labels.contains("main")) {
    std::cerr << "Error: Label \"main\" not defined.\n";
    return EXIT_FAILURE;
  }

  assemble(out, global_labels.at("main"), instructions, global_labels);
  std::cout << "Assembled.\n";
}

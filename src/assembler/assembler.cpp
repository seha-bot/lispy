#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "bytecoder.hpp"
#include "instr_ptr.hpp"
#include "mnemonic.hpp"

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

    std::unordered_map<std::string, std::size_t> atoms;
    auto encode_atom = [&atoms](std::string const& name) { return atoms.emplace(name, atoms.size()).first->second; };

    std::unordered_map<std::string, std::size_t> global_label_positions;
    std::vector<bytecoder::Instruction> instructions;
    std::vector<std::pair<std::size_t, std::string>> unplaced_jumps;

    int line_num = 0;
    auto fail = [&line_num] [[noreturn]] (char const *msg) {
        std::cerr << "Error at line " << line_num << ": " << msg << '\n';
        std::exit(EXIT_FAILURE);
    };

    std::vector<std::size_t> instr_size_prefix_sum{0};

    while (true) {
        ++line_num;
        std::string line;
        if (not std::getline(in, line)) {
            break;
        }

        if (line.empty()) {
            continue;
        }

        std::istringstream sline(line);

        if (line[0] != '\t') {
            std::string name;
            sline >> name;
            if (name.back() == ':') {
                name.pop_back();
                global_label_positions[std::move(name)] = instructions.size();
                continue;
            }
            global_label_positions[name] = instructions.size();

            std::vector<std::string> parameters;
            std::string parameter;
            while (sline >> parameter) {
                parameters.push_back(std::move(parameter));
            }
            if (parameters.empty() or parameters.back().back() != ':') {
                fail("Labels must end with ':'.");
            }
            parameters.back().pop_back();
            continue;
        }

        auto mnemonic = [&] {
            std::string raw;
            sline >> raw;
            return from_string(raw);
        }();
        if (not mnemonic) {
            fail("Unknown mnemonic.");
        }

        auto operand = [&] -> std::optional<bytecoder::Operand> {
            if (not(sline >> std::ws) or sline.peek() == ';') {
                return std::nullopt;
            }

            if (sline.peek() == '\'') {
                sline.get();
                std::string name;
                sline >> name;
                std::uint64_t const atom_index = encode_atom(name);
                return bytecoder::Operand(bytecoder::OperandType::atom, bytecoder::size_from_unsigned(atom_index),
                                          atom_index);
            } else if (sline.peek() == '[') {
                sline.get();
                std::uint64_t index = 0;
                sline >> index;
                sline.get();
                return bytecoder::Operand(bytecoder::OperandType::stack, bytecoder::size_from_unsigned(index), index);
            } else if (std::isdigit(sline.peek()) or sline.peek() == '-') {
                std::int64_t val = 0;
                sline >> val;
                // TODO: signedness depends on the mnemonic
                auto size = bytecoder::size_from_signed(val);
                return bytecoder::Operand(bytecoder::OperandType::literal, size, val);
            } else {
                std::string jump_to_name;
                sline >> jump_to_name;
                unplaced_jumps.emplace_back(instructions.size(), jump_to_name);
                return bytecoder::Operand(bytecoder::OperandType::literal, bytecoder::OperandSize::four_B, 0);
            }
        }();

        instructions.push_back(bytecoder::Instruction{*mnemonic, operand});
        instr_size_prefix_sum.push_back(instr_size_prefix_sum.back() + bytecoder::instr_size(instructions.back()));
    }

    for (auto [from, to] : unplaced_jumps) {
        auto it = global_label_positions.find(to);
        if (it == global_label_positions.end()) {
            std::cerr << "Undefined function: " << to << '\n';
            return EXIT_FAILURE;
        }

        switch (instructions[from].mnemonic) {
            case Mnemonic::push:
            case Mnemonic::closure: {
                auto ip = instr_size_prefix_sum[it->second];
                instructions[from].operand =
                    bytecoder::Operand(bytecoder::OperandType::literal, bytecoder::OperandSize::four_B, ip);
                break;
            }
            default:
                auto offset = static_cast<std::int64_t>(instr_size_prefix_sum[it->second]) -
                              static_cast<std::int64_t>(instr_size_prefix_sum[from]);
                instructions[from].operand =
                    bytecoder::Operand(bytecoder::OperandType::literal, bytecoder::OperandSize::four_B, offset);
        }
    }

    std::vector<std::string> ordered_atoms(atoms.size());
    for (auto& [name, i] : atoms) {
        ordered_atoms[i] = name;
    }

    std::optional<InstrPtr> entry_point;
    if (auto it = global_label_positions.find("main"); it != global_label_positions.end()) {
        entry_point = InstrPtr(instr_size_prefix_sum[it->second]);
    }

    bytecoder::assemble(out, ordered_atoms, entry_point, instructions);
    std::cout << "Assembled.\n";
}

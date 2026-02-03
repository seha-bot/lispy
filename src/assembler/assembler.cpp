#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "mnemonic.hpp"

// TODO: this also exists in decoder.hpp
// Rename Decoder to CoDec and have most of this file be there.
enum class OperandType { atom, literal, stack };

void push_n(std::vector<unsigned char>& buf, std::uint64_t val, std::size_t n) {
    auto bits = static_cast<std::uint64_t>(val);
    for (std::size_t i = 0; i < n; ++i) {
        auto x = (bits >> (((n - 1) - i) * 8)) & 0xFF;
        buf.push_back(static_cast<unsigned char>(x));
    }
};

void write_n(std::vector<unsigned char>& buf, std::size_t at, std::uint64_t val, std::size_t n) {
    auto bits = static_cast<std::uint64_t>(val);
    for (std::size_t i = 0; i < n; ++i) {
        auto x = (bits >> (((n - 1) - i) * 8)) & 0xFF;
        buf[at + i] = static_cast<unsigned char>(x);
    }
};

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

    std::unordered_map<std::string, std::size_t> global_label_positions;
    std::vector<std::string> atoms;
    std::vector<std::pair<std::size_t, std::string>> unplaced_jumps;
    std::vector<unsigned char> bytecode;
    int line_num = 0;

    while (true) {
        ++line_num;
        std::string line;
        if (not std::getline(in, line)) {
            break;
        }

        if (line.empty()) {
            continue;
        }

        if (line[0] != '\t') {
            auto end = std::ranges::find(line, ':');
            global_label_positions[std::string(line.begin(), end)] = bytecode.size();
            continue;
        }

        std::istringstream sline(line);
        std::string ms;
        sline >> ms;
        auto mnemonic = from_string(ms);
        if (not mnemonic) {
            std::cerr << "Unknown mnemonic at line " << line_num << '\n';
            return EXIT_FAILURE;
        }
        std::size_t i_opcode = bytecode.size();
        bytecode.push_back(static_cast<unsigned char>(*mnemonic));

        for (int i = 0; i < 2; ++i) {
            std::int64_t val;
            std::string str;

            if (not(sline >> std::ws)) {
                break;
            }

            if (sline.peek() == ';') {
                break;
            }

            bytecode[i_opcode] = bytecode[i_opcode] & 0b00111111;

            if (sline.peek() == '\'') {
                sline.get();
                sline >> str;

                if (auto it = std::ranges::find(atoms, str); it != atoms.end()) {
                    push_n(bytecode, static_cast<std::int64_t>(it - atoms.begin()), 8);
                } else {
                    push_n(bytecode, static_cast<std::int64_t>(atoms.size()), 8);
                    atoms.push_back(str);
                }

                bytecode[i_opcode] |= static_cast<unsigned char>(OperandType::atom) << 6;
            } else if (sline.peek() == '[') {
                sline.get();
                sline >> val;
                push_n(bytecode, val, 8);
                sline.get();
                bytecode[i_opcode] |= static_cast<unsigned char>(OperandType::stack) << 6;
            } else if (std::isdigit(sline.peek())) {
                sline >> val;
                push_n(bytecode, val, 8);
                bytecode[i_opcode] |= static_cast<unsigned char>(OperandType::literal) << 6;
            } else {
                sline >> str;
                unplaced_jumps.emplace_back(i_opcode, str);
                switch (*mnemonic) {
                    case Mnemonic::push:
                    case Mnemonic::closure:
                        push_n(bytecode, 0, 8);
                        break;
                    default:
                        push_n(bytecode, 0, 2);
                        break;
                }
                bytecode[i_opcode] |= static_cast<unsigned char>(OperandType::literal) << 6;
            }
        }
    }

    std::vector<unsigned char> prefix;
    push_n(prefix, 0, 8);  // entry point slot

    for (auto& atom : atoms) {
        prefix.append_range(atom);
        prefix.push_back('\0');
    }
    prefix.push_back('\0');

    if (auto it = global_label_positions.find("main"); it != global_label_positions.end()) {
        write_n(prefix, 0, prefix.size() + it->second, 8);
    }

    for (auto [from, to] : unplaced_jumps) {
        auto it = global_label_positions.find(to);
        if (it == global_label_positions.end()) {
            std::cerr << "Undefined function: " << to << '\n';
            return EXIT_FAILURE;
        }

        switch (Mnemonic(bytecode[from] & 0b00111111)) {
            case Mnemonic::push:
            case Mnemonic::closure:
                // WARN: the plus 1 will make you suffer when multibyte opcodes are introduced.
                write_n(bytecode, from + 1, prefix.size() + it->second, 8);
                break;
            default:
                // If you ever get the need to jump farther than 2^16 bits
                // (which you will), here is the place to pick if a jump is near or far.
                auto offset = it->second - from;
                // WARN: the plus 1 will make you suffer when multibyte opcodes are introduced.
                write_n(bytecode, from + 1, offset, 2);
        }
    }

    for (auto b : prefix) {
        out << b;
    }
    for (auto b : bytecode) {
        out << b;
    }

    std::cout << "Assembled.\n";
}

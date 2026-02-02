#ifndef DECODER_HPP
#define DECODER_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "instr_ptr.hpp"
#include "mnemonic.hpp"

// Since this is coded as 00 or 01 or 10, then 11 can be used to
// denote a second byte for opcodes.
enum class OperandType { atom, literal, stack };

struct Operand {
    OperandType type{};
    std::int64_t value{};
};

struct Decoder {
    Mnemonic get_opcode(InstrPtr ip) const { return Mnemonic(bytes[ip.value()] & 0b00111111); }

    std::int16_t get_offset(InstrPtr ip) const {
        auto high = static_cast<std::uint16_t>(bytes[ip.value() + 1]);
        auto low = static_cast<std::uint16_t>(bytes[ip.value() + 2]);
        return static_cast<std::int16_t>(high << 8 | low);
    }

    Operand get_operand(InstrPtr ip, std::size_t offset) const {
        std::uint64_t bits = 0;
        bits = (bits << 8) | bytes[ip.value() + offset + 0];
        bits = (bits << 8) | bytes[ip.value() + offset + 1];
        bits = (bits << 8) | bytes[ip.value() + offset + 2];
        bits = (bits << 8) | bytes[ip.value() + offset + 3];
        bits = (bits << 8) | bytes[ip.value() + offset + 4];
        bits = (bits << 8) | bytes[ip.value() + offset + 5];
        bits = (bits << 8) | bytes[ip.value() + offset + 6];
        bits = (bits << 8) | bytes[ip.value() + offset + 7];
        return Operand{OperandType(bytes[ip.value()] >> 6), static_cast<std::int64_t>(bits)};
    }

    Operand get_object(InstrPtr ip) const { return get_operand(ip, 1); }
    Operand get_object2(InstrPtr ip) const { return get_operand(ip, 9); }

    static constexpr std::int32_t instr_size(Mnemonic m) {
        switch (m) {
            case Mnemonic::eq:
                return 1;
            case Mnemonic::cons:
                return 1;
            case Mnemonic::car:
                return 1;
            case Mnemonic::cdr:
                return 1;
            case Mnemonic::jt:
                return 3;
            case Mnemonic::jf:
                return 3;
            case Mnemonic::jmp:
                return 3;
            case Mnemonic::call:
                return 3;
            case Mnemonic::indjmp:
                return 9;
            case Mnemonic::indcall:
                return 9;
            case Mnemonic::push:
                return 9;
            case Mnemonic::pop:
                return 9;
            case Mnemonic::ret:
                return 9;
            case Mnemonic::set:
                return 17;
            case Mnemonic::print:
                return 1;
            case Mnemonic::mkstr:
                return 1;
            case Mnemonic::strpush:
                return 9;
            case Mnemonic::closure:
                return 9;
            case Mnemonic::capture:
                return 9;
        }
    }

    std::vector<unsigned char> bytes;
};

#endif

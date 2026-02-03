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

    Operand operand_impl(InstrPtr ip, std::size_t offset) const {
        std::uint64_t bits = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            bits = (bits << 8) | bytes[ip.value() + offset + i];
        }
        return Operand{OperandType(bytes[ip.value()] >> 6), static_cast<std::int64_t>(bits)};
    }

    Operand operand1(InstrPtr ip) const { return operand_impl(ip, 1); }
    Operand operand2(InstrPtr ip) const { return operand_impl(ip, 9); }

    InstrPtr entry_point() const { return InstrPtr(static_cast<std::size_t>(operand_impl(InstrPtr(0), 0).value)); }

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

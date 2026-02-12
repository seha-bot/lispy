#ifndef BYTECODER_HPP
#define BYTECODER_HPP

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "instr_ptr.hpp"
#include "mnemonic.hpp"

// TODO: this file was written quickly and there wasn't much thought put into it.
// I honestly don't like the API, so work on this more.

namespace bytecoder {

// Since this is coded as 00 or 01 or 10, then 11 can be used to
// denote a second byte for opcodes.
enum class OperandType { atom, literal, stack };

enum class OperandSize { one_B, two_B, four_B, eight_B };

inline OperandSize size_from_unsigned(std::uint64_t value) {
    if (value <= 255) {
        return OperandSize::one_B;
    } else if (value <= 65535) {
        return OperandSize::two_B;
    } else if (value <= 4294967295) {
        return OperandSize::four_B;
    } else {
        return OperandSize::eight_B;
    }
}

inline OperandSize size_from_signed(std::int64_t value) {
    if (-128 <= value && value <= 127) {
        return OperandSize::one_B;
    } else if (-32768 <= value && value <= 32767) {
        return OperandSize::two_B;
    } else if (-2147483648 <= value && value <= 2147483647) {
        return OperandSize::four_B;
    } else {
        return OperandSize::eight_B;
    }
}

inline std::size_t size_value(OperandSize size) { return std::size_t(1) << static_cast<int>(size); }

struct Operand {
    Operand(OperandType type, OperandSize size, auto value)
        : m_type(type), m_size(size), m_value(static_cast<std::uint64_t>(value)) {}

    std::size_t size_value() const { return bytecoder::size_value(m_size); }
    std::int64_t value_signed() const { return static_cast<std::int64_t>(m_value); }
    std::uint64_t value_unsigned() const { return m_value; }
    OperandType type() const { return m_type; }
    OperandSize size() const { return m_size; }

private:
    OperandType m_type{};
    OperandSize m_size{};
    std::uint64_t m_value{};
};

struct Instruction {
    Mnemonic mnemonic;
    std::optional<Operand> operand;
};

inline std::size_t instr_size(Instruction instr) {
    switch (instr.mnemonic) {
        case Mnemonic::eq:
        case Mnemonic::cons:
        case Mnemonic::car:
        case Mnemonic::cdr:
        case Mnemonic::pop:
        case Mnemonic::print:
        case Mnemonic::mkstr:
        case Mnemonic::iadd:
        case Mnemonic::ineg:
        case Mnemonic::iless:
        case Mnemonic::imod:
            return 1;
        case Mnemonic::jt:
        case Mnemonic::jf:
        case Mnemonic::jmp:
        case Mnemonic::call:
        case Mnemonic::indjmp:
        case Mnemonic::indcall:
        case Mnemonic::push:
        case Mnemonic::ret:
        case Mnemonic::set:
        case Mnemonic::strpush:
        case Mnemonic::closure:
        case Mnemonic::capture:
            return 2 + instr.operand.value().size_value();
    }
    std::unreachable();
}

// TODO: move this function outside of this file.
// In order to do that, you have to make this file provide appropriate tools for the job.
inline void assemble(std::ostream& os, std::vector<std::string> const& atoms, std::optional<InstrPtr> entry_point,
                     std::vector<Instruction> const& instrs) {
    auto write_n = [&os](std::uint64_t val, std::size_t n) {
        for (std::size_t i = 0; i < n; ++i) {
            os.put(static_cast<char>((val >> ((n - 1 - i) * 8)) & 0xFF));
        }
    };

    write_n(entry_point ? entry_point->value() : 0, 8);

    for (auto& atom : atoms) {
        os << atom;
        os << '\0';
    }
    os << '\0';

    for (auto [mnemonic, operand] : instrs) {
        write_n(static_cast<std::uint64_t>(mnemonic), 1);
        if (operand) {
            // xx xx xxxx
            // ^  ^  ^
            // |  |  reserved
            // |  size
            // type
            std::uint8_t operand_data = 0;
            operand_data |= static_cast<std::uint8_t>(static_cast<unsigned>(operand->type()) << 6);
            operand_data |= static_cast<std::uint8_t>(static_cast<unsigned>(operand->size()) << 4);
            write_n(operand_data, 1);
            write_n(operand->value_unsigned(), operand->size_value());
        }
    }
}

inline std::uint64_t read_n(std::istream& is, std::size_t n) {
    std::uint64_t res = 0;
    for (std::size_t i = 0; i < n; ++i) {
        res <<= 8;
        res |= is.get();
    }
    return res;
}

struct Prefix {
    InstrPtr entry_point;
    std::vector<std::string> atoms;
};

inline Prefix read_prefix(std::istream& is) {
    InstrPtr entry_point(static_cast<std::size_t>(read_n(is, 8)));
    std::vector<std::string> atoms;
    while (is.peek() != '\0') {
        atoms.emplace_back();
        while (is.peek() != '\0') {
            atoms.back().push_back(is.get());
        }
        is.get();
    }
    is.get();
    return Prefix{entry_point, std::move(atoms)};
}

using Bytecode = std::vector<std::uint8_t>;

inline std::uint64_t read_n(Bytecode const& bc, std::size_t at, std::size_t n) {
    std::uint64_t res = 0;
    for (std::size_t i = 0; i < n; ++i) {
        res <<= 8;
        res |= bc[at + i];
    }
    return res;
}

inline Mnemonic opcode(Bytecode const& bc, InstrPtr ip) { return Mnemonic(read_n(bc, ip.value(), 1)); }

inline Operand operand(Bytecode const& bc, InstrPtr ip) {
    auto operand_data = read_n(bc, ip.value() + 1, 1);
    auto type = static_cast<OperandType>((operand_data >> 6) & 0b11);
    auto size = static_cast<OperandSize>((operand_data >> 4) & 0b11);
    auto value = read_n(bc, ip.value() + 2, size_value(size));
    switch (size) {
        case OperandSize::one_B:
            value = static_cast<std::uint64_t>(static_cast<std::int8_t>(value));
            break;
        case OperandSize::two_B:
            value = static_cast<std::uint64_t>(static_cast<std::int16_t>(value));
            break;
        case OperandSize::four_B:
            value = static_cast<std::uint64_t>(static_cast<std::int32_t>(value));
            break;
        case OperandSize::eight_B:
            break;
    }
    return Operand(type, size, value);
}

}  // namespace bytecoder

#endif

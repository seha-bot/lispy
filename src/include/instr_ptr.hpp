#ifndef INSTR_PTR_HPP
#define INSTR_PTR_HPP

#include <cstddef>
#include <cstdint>

/// @brief Represents an index of an instruction inside bytecode.
struct InstrPtr {
    explicit InstrPtr(std::size_t value) : m_value(value) {}

    InstrPtr offset(std::int64_t offset) const { return InstrPtr(m_value + static_cast<std::size_t>(offset)); }
    std::size_t value() const { return m_value; }

private:
    std::size_t m_value;
};

#endif

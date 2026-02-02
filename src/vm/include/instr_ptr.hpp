#ifndef INSTR_PTR_HPP
#define INSTR_PTR_HPP

#include <cstddef>
#include <cstdint>

struct InstrPtr {
    explicit InstrPtr(std::size_t value) : m_value(value) {}

    InstrPtr offset(std::int64_t offset) const { return InstrPtr(m_value + offset); }
    InstrPtr next() const { return InstrPtr(m_value + 1); }
    std::size_t value() const { return m_value; }

private:
    std::size_t m_value;
};

#endif

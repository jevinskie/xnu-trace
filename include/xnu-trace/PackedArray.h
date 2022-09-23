#pragma once

#include "common.h"

#include <memory>
#include <type_traits>

template <uint8_t NBits>
using uint_n = decltype([]() {
    if constexpr (NBits > 0 && NBits <= 8) {
        return uint8_t{};
    } else if constexpr (NBits > 8 && NBits <= 16) {
        return uint16_t{};
    } else if constexpr (NBits > 16 && NBits <= 32) {
        return uint32_t{};
    } else if constexpr (NBits > 32 && NBits <= 64) {
        return uint64_t{};
    } else {
        return;
    }
}());

class XNUTRACE_EXPORT PackedArray {
public:
    class reference {
    public:
        reference(PackedArray &pa, size_t pos);
        operator uint64_t() const;
        reference &operator=(uint64_t val);

    private:
        const size_t m_pos;
        PackedArray &m_pa;
    };

    using const_reference = const reference;

    PackedArray(size_t sz, uint8_t nbits);
    constexpr reference operator[](size_t pos);
    constexpr const_reference operator[](size_t pos) const;

private:
    std::unique_ptr<uint8_t[]> m_buf;
    size_t m_sz;
    uint8_t m_nbits;
};

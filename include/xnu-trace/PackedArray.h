#pragma once

#include "common.h"

#include <type_traits>

template <size_t NBits>
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

static_assert(std::is_same_v<uint_n<0>, void>, "");
static_assert(std::is_same_v<uint_n<5>, uint8_t>, "");
static_assert(std::is_same_v<uint_n<11>, uint16_t>, "");
static_assert(std::is_same_v<uint_n<65>, void>, "");

class XNUTRACE_EXPORT PackedArray {
public:
    PackedArray(size_t sz, size_t nbits);
    ~PackedArray();

private:
    uint8_t *m_buf;
    size_t m_sz;
    uint8_t m_nbits;
};

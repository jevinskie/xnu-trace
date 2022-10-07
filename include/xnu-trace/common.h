#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "common-c.h"

#define static_assert_cond(cond) static_assert((cond), #cond)

template <typename T> consteval size_t sizeofbits() {
    return sizeof(T) * 8;
}

consteval size_t sizeofbits(const auto &o) {
    return sizeof(o) * 8;
}

template <typename T>
concept POD = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

using uint128_t = __uint128_t;
using int128_t  = __int128_t;

constexpr uint64_t PAGE_SZ      = 4 * 1024;
constexpr uint64_t PAGE_SZ_LOG2 = 12;
constexpr uint64_t PAGE_SZ_MASK = 0xfff;

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
    } else if constexpr (NBits > 64 && NBits <= 128) {
        return uint128_t{};
    } else {
        XNUTRACE_UNREACHABLE();
        return;
    }
}());

template <size_t NBits>
using sint_n = decltype([]() {
    if constexpr (NBits > 0 && NBits <= 8) {
        return int8_t{};
    } else if constexpr (NBits > 8 && NBits <= 16) {
        return int16_t{};
    } else if constexpr (NBits > 16 && NBits <= 32) {
        return int32_t{};
    } else if constexpr (NBits > 32 && NBits <= 64) {
        return int64_t{};
    } else if constexpr (NBits > 64 && NBits <= 128) {
        return int128_t{};
    } else {
        XNUTRACE_UNREACHABLE();
        return;
    }
}());

template <size_t NBits, bool Signed>
using int_n = decltype([]() {
    if constexpr (Signed) {
        return sint_n<NBits>{};
    } else {
        return uint_n<NBits>{};
    }
});

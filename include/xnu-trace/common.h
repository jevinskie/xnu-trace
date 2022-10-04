#pragma once

#include <cstdint>
#include <type_traits>
#include <utility>

#include "common-c.h"

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
    } else {
        XNUTRACE_UNREACHABLE();
        return;
    }
}());

template <size_t NBits>
using int_n = decltype([]() {
    if constexpr (NBits > 0 && NBits <= 8) {
        return int8_t{};
    } else if constexpr (NBits > 8 && NBits <= 16) {
        return int16_t{};
    } else if constexpr (NBits > 16 && NBits <= 32) {
        return int32_t{};
    } else if constexpr (NBits > 32 && NBits <= 64) {
        return int64_t{};
    } else {
        XNUTRACE_UNREACHABLE();
        return;
    }
}());

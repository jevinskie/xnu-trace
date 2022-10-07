#pragma once

#include <cstdint>
#include <string_view>
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

template <typename T> constexpr auto type_name() {
    std::string_view name, prefix, suffix;
#ifdef __clang__
    name   = __PRETTY_FUNCTION__;
    prefix = "auto type_name() [T = ";
    suffix = "]";
#elif defined(__GNUC__)
    name   = __PRETTY_FUNCTION__;
    prefix = "constexpr auto type_name() [with T = ";
    suffix = "]";
#elif defined(_MSC_VER)
    name   = __FUNCSIG__;
    prefix = "auto __cdecl type_name<";
    suffix = ">(void)";
#endif
    name.remove_prefix(prefix.size());
    name.remove_suffix(suffix.size());
    return name;
}

template <typename T>
concept POD = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

using uint128_t = __uint128_t;
using int128_t  = __int128_t;

constexpr uint64_t PAGE_SZ      = 4 * 1024;
constexpr uint64_t PAGE_SZ_LOG2 = 12;
constexpr uint64_t PAGE_SZ_MASK = 0xfff;

template <size_t NBits>
using uint_n = std::conditional_t<
    (NBits > 0 && NBits <= 8), uint8_t,
    std::conditional_t<
        (NBits > 8 && NBits <= 16), uint16_t,
        std::conditional_t<(NBits > 16 && NBits <= 32), uint32_t,
                           std::conditional_t<(NBits > 32 && NBits <= 64), uint64_t,
                                              std::conditional_t<(NBits > 64 && NBits <= 128),
                                                                 uint128_t, void>>>>>;

template <size_t NBits>
using sint_n = std::conditional_t<
    (NBits > 0 && NBits <= 8), int8_t,
    std::conditional_t<
        (NBits > 8 && NBits <= 16), int16_t,
        std::conditional_t<
            (NBits > 16 && NBits <= 32), int32_t,
            std::conditional_t<(NBits > 32 && NBits <= 64), int64_t,
                               std::conditional_t<(NBits > 64 && NBits <= 128), int128_t, void>>>>>;

template <size_t NBits, bool Signed>
using int_n = std::conditional_t<Signed, sint_n<NBits>, uint_n<NBits>>;

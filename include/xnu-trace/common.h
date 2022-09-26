#pragma once

#include <cstdint>
#include <type_traits>

#include "common-c.h"

template <typename T>
concept POD = std::is_trivial_v<T> && std::is_standard_layout_v<T>;

using uint128_t = __uint128_t;

constexpr uint64_t PAGE_SZ      = 4 * 1024;
constexpr uint64_t PAGE_SZ_LOG2 = 12;
constexpr uint64_t PAGE_SZ_MASK = 0xfff;

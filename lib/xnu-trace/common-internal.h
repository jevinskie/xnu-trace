#pragma once

#undef NDEBUG
#include <cassert>
#include <concepts>

// #include <mach-o/dyld_images.h>
// #include <mach-o/loader.h>

#include <BS_thread_pool.hpp>
#include <fmt/format.h>

namespace fs = std::filesystem;
using namespace std::string_literals;

#if 0
template <typename T> size_t bytesizeof(const typename std::vector<T> &vec) {
    return sizeof(T) * vec.size();
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U roundup_pow2_mul(U num, std::size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return (num + mask) & ~mask;
}

template <typename U>
requires requires() {
    requires std::unsigned_integral<U>;
}
constexpr U rounddown_pow2_mul(U num, std::size_t pow2_mul) {
    const U mask = static_cast<U>(pow2_mul) - 1;
    return num & ~mask;
}

// utils.cpp


// mach.cpp

// macho.cpp

#endif

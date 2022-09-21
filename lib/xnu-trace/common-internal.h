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

double timespec_diff(const timespec &a, const timespec &b);
std::string prot_to_str(vm_prot_t prot);
std::string block_str(double percentage, unsigned int width = 80);

// mach.cpp

// macho.cpp

std::vector<segment_command_64> read_macho_segs_target(task_t target_task, uint64_t macho_hdr_addr);
std::vector<segment_command_64> read_macho_segs_target(task_t target_task,
                                                       const mach_header_64 *macho_hdr);
uint64_t get_text_size(const std::vector<segment_command_64> &segments);
uint64_t get_text_base(const std::vector<segment_command_64> &segments);
#endif
